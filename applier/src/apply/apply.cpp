#include "applier/apply.h"
#include "applier/utility.h"
#include "applier/buffer_pool.h"
#include "applier/parse.h"
#include "applier/logger.h"
#include "applier/file_io.h"
#include "storpu/storpu.h"
namespace Lemon {

ApplySystem::ApplySystem() {
  // 1.填充meta_data_buf
  flash_read((LOG_PARTITION * PARTITION_SIZE) / FLASH_PAGE_SIZE, 1, meta_data_buf_);

  // 2.设置checkpoint_lsn和checkpoint_no
  uint32_t checkpoint_no_1 = mach_read_from_8(meta_data_buf_ + 1 * LOG_BLOCK_SIZE + LOG_CHECKPOINT_NO);
  uint32_t checkpoint_no_2 = mach_read_from_8(meta_data_buf_ + 3 * LOG_BLOCK_SIZE + LOG_CHECKPOINT_NO);
  if (checkpoint_no_1 > checkpoint_no_2) {
    checkpoint_no_ = checkpoint_no_1;
    checkpoint_lsn_ = mach_read_from_8(meta_data_buf_ + 1 * LOG_BLOCK_SIZE + LOG_CHECKPOINT_LSN);
    checkpoint_offset_ = mach_read_from_8(meta_data_buf_ + 1 * LOG_BLOCK_SIZE + LOG_CHECKPOINT_OFFSET);
  } else {
    checkpoint_no_ = checkpoint_no_2;
    checkpoint_lsn_ = mach_read_from_8(meta_data_buf_ + 3 * LOG_BLOCK_SIZE + LOG_CHECKPOINT_LSN);
    checkpoint_offset_ = mach_read_from_8(meta_data_buf_ + 3 * LOG_BLOCK_SIZE + LOG_CHECKPOINT_OFFSET);
  }

  LOG_DEBUG("Initialized apply system, checkpoint_no: %d, checkpoint_lsn: %d, checkpoint_offset: %d.\n",
            checkpoint_no_, checkpoint_lsn_, checkpoint_offset_);
}

ApplySystem::~ApplySystem() {
  munmap(parse_buf_, parse_buf_size_ * 2);
  parse_buf_ = nullptr;
  munmap(meta_data_buf_, meta_data_buf_size_);
  meta_data_buf_ = nullptr;
}

bool ApplySystem::PopulateHashMap() {


  if (next_fetch_page_id_ > log_max_page_id_) {
    spu_printf("we have processed all redo log.\n");
    return false;
  }

  byte *buf = (byte *) mmap(nullptr,
                            DATA_PAGE_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG,
                            -1,
                            0);

  // 1.填充parse buffer
  uint32_t end_page_id = next_fetch_page_id_ + (parse_buf_size_ - parse_buf_content_size_) / DATA_PAGE_SIZE;
  for (; next_fetch_page_id_ < end_page_id; ++next_fetch_page_id_) {

    // 读一个Page放到buf里面去
    flash_read(next_fetch_page_id_, 1, buf);

    for (int block = 0; block < static_cast<int>((DATA_PAGE_SIZE / LOG_BLOCK_SIZE)); ++block) {
      if (next_fetch_page_id_ == 0 && block < 4) continue; // 跳过前面4个block
      auto hdr_no = ~LOG_BLOCK_FLUSH_BIT_MASK & mach_read_from_4(buf + block * LOG_BLOCK_SIZE);
      auto data_len = mach_read_from_2(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_HDR_DATA_LEN);
      auto first_rec = mach_read_from_2(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_FIRST_REC_GROUP);
      auto checkpoint_no = mach_read_from_4(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_CHECKPOINT_NO);
      auto checksum = mach_read_from_4(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_CHECKSUM);
      // 这个block还没装满
      if (data_len != 512) {
        spu_printf("(page_id = %d, block = %d) was not filled.\n", next_fetch_page_id_, block);
        return false;
      }
      // 每个block的日志掐头去尾放到parse buffer中
      uint32_t len = data_len - LOG_BLOCK_HDR_SIZE - LOG_BLOCK_TRL_SIZE;
      memcpy(parse_buf_ptr_ + parse_buf_content_size_,
                  buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_HDR_SIZE, len);
      parse_buf_content_size_ += len;
    }
  }

  // 2.从parse buffer中循环解析日志，放到哈希表中
  unsigned char *end_ptr = parse_buf_ptr_ + parse_buf_content_size_;
  unsigned char *start_ptr = parse_buf_ptr_;
  while (start_ptr < end_ptr) {
    uint32_t len = 0, space_id, page_id;
    LOG_TYPE	type;
    byte *log_body_ptr = nullptr;
    len = ParseSingleLogRecord(type, start_ptr, end_ptr, space_id, page_id, &log_body_ptr);

    if (len == 0) {
      break;
    }
    // 加入哈希表

    hash_map_[space_id][page_id].emplace_back(type, space_id, page_id,
                                              next_lsn_, len, log_body_ptr,
                                              start_ptr + len);

    start_ptr += len;
    next_lsn_ = recv_calc_lsn_on_data_add(next_lsn_, len);
  }


  // 把没有解析完成的日志移动到另一个缓冲区
  parse_buf_content_size_ = end_ptr - start_ptr;
  // 双buffer切换
  parse_buf_ptr_ = parse_buf_ + (parse_buf_ptr_ - parse_buf_ + parse_buf_size_) % (parse_buf_size_ * 2);
  memcpy(parse_buf_ptr_, start_ptr, parse_buf_content_size_);
  return true;
}
bool ApplySystem::ApplyHashLogs() {
  if (hash_map_.empty()) return false;

  auto iter_begin = hash_map_.begin();
  auto iter_end = hash_map_.end();

  for (; iter_begin != iter_end; ++iter_begin) {

    auto space_id = iter_begin->get<0>();

    if (space_id != 26) {
      continue;
    }

    auto iter2_begin = iter_begin->get<1>().begin();
    auto iter2_end = iter_begin->get<1>().end();
    for ( ; iter2_begin != iter2_end; ++iter2_begin) {

      auto page_id = iter2_begin->get<0>();

      // 获取需要的page
      Page *page = buffer_pool_->GetPage(space_id, page_id);
      LOG_DEBUG("fetch one page from buffer pool, space_id = %d, page_id = %d, page_lsn = %d.\n",
                page->GetSpaceId(), page->GetPageId(), page->GetLSN());

      lsn_t page_lsn = page->GetLSN();

      auto &log_list = iter2_begin->get<1>();
      while (!log_list.empty()) {
        auto &log = log_list.front();

        if (log.type_ == LOG_TYPE::MLOG_COMP_PAGE_CREATE) {
          page = buffer_pool_->NewPage(space_id, page_id);
          LOG_DEBUG("create new page from buffer pool, space_id = %d, page_id = %d, page_lsn = %d.\n",
                    page->GetSpaceId(), page->GetPageId(), page->GetLSN());
        }

        lsn_t log_lsn = log.log_start_lsn_;
//        // 从checkpoint点之后开始apply
//        if (log_lsn <= checkpoint_lsn_) {
//          continue;
//        }
//        // skip!
//        if (page_lsn > log_lsn) {
//          continue;
//        }

        if (ApplyOneLog(page, log)) {
          page->WritePageLSN(log_lsn + log.log_len_);
          page->WriteCheckSum(BUF_NO_CHECKSUM_MAGIC);
          LOG_DEBUG("applied one log type = %s, space_id = %d, page_id = %d, log_len = %d, lsn = %d.\n",
                    GetLogString(log.type_), log.space_id_, log.page_id_, log.log_len_, log.log_start_lsn_);
        } else {
          LOG_DEBUG("unsupported log type = %s, space_id = %d, page_id = %d, log_len = %d, lsn = %d, skip apply.\n",
                    GetLogString(log.type_), log.space_id_, log.page_id_, log.log_len_, log.log_start_lsn_);
        }
        log_list.pop_front();
      }
    }
  }
  return true;
}

bool ApplySystem::ApplyOneLog(Page *page, const LogEntry &log) {
  byte *ret;
  switch (log.type_) {
    case MLOG_1BYTE:
    case MLOG_2BYTES:
    case MLOG_4BYTES:
    case MLOG_8BYTES:
      ret = ParseOrApplyNBytes(log.type_, log.log_body_start_ptr_, log.log_body_end_ptr_, page->GetData());
      return ret != nullptr;
    case MLOG_WRITE_STRING:
      ret = ParseOrApplyString(log.log_body_start_ptr_, log.log_body_end_ptr_, page->GetData());
      return ret != nullptr;
    case MLOG_COMP_PAGE_CREATE:
      ret = ApplyCompPageCreate(page->GetData());
      return ret != nullptr;
    case MLOG_INIT_FILE_PAGE2:
      return ApplyInitFilePage2(log, page);
    case MLOG_COMP_REC_INSERT:
      return ApplyCompRecInsert(log, page);
    case MLOG_COMP_REC_CLUST_DELETE_MARK:
      return ApplyCompRecClusterDeleteMark(log, page);
    case MLOG_REC_SEC_DELETE_MARK:
      return ApplyRecSecondDeleteMark(log, page);
    case MLOG_COMP_REC_SEC_DELETE_MARK:
      return ApplyCompRecSecondDeleteMark(log, page);
    case MLOG_COMP_REC_UPDATE_IN_PLACE:
      return ApplyCompRecUpdateInPlace(log, page);
    case MLOG_COMP_REC_DELETE:
      return ApplyCompRecDelete(log, page);
    case MLOG_COMP_LIST_END_COPY_CREATED:
      return ApplyCompListEndCopyCreated(log, page);
    case MLOG_COMP_PAGE_REORGANIZE:
      return ApplyCompPageReorganize(log, page);
    case MLOG_COMP_LIST_START_DELETE:
    case MLOG_COMP_LIST_END_DELETE:
      return ApplyCompListDelete(log, page);
    case MLOG_IBUF_BITMAP_INIT:
      return ApplyIBufBitmapInit(log, page);
    default:
      // skip
//      std::cout << "We can not apply " << GetLogString(log.type_) << ", just skipped." << std::endl;
      return false;
  }
  return true;
}
}

