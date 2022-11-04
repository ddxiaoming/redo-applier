#include <iostream>
#include <thread>
#include <cstring>
#include "apply.h"
#include "utility.h"
#include "buffer_pool.h"
#include "chrono"
#include "parse.h"
namespace Lemon {
static int logs_applied = 0;
static unsigned long long read_file_time_in_parse = 0; // nano seconds
static unsigned long long read_file_time_in_apply = 0; // nano seconds
static unsigned long long apply_time = 0; // nano seconds
static unsigned long long parse_time = 0; // nano seconds
static unsigned long long total_time = 0; // nano seconds
static unsigned long long apply_file_len = 0; // bytes
static unsigned long long parse_file_len = 0; // bytes
ApplySystem::ApplySystem(bool save_logs) :
    hash_map_(),
    parse_buf_size_(10 * 1024 * 1024), // 10M
    parse_buf_(new unsigned char[parse_buf_size_ * 2]),
    parse_buf_ptr_(parse_buf_),
    parse_buf_content_size_(0),
    meta_data_buf_size_(LOG_BLOCK_SIZE * N_LOG_METADATA_BLOCKS), // 4 blocks
    meta_data_buf_(new unsigned char[meta_data_buf_size_]),
    checkpoint_lsn_(0),
    checkpoint_no_(0),
    checkpoint_offset_(0),
    log_file_size_(static_cast<uint64_t>(1) * 1024 * 1024 * 1024), // 1GB
    next_fetch_page_id_(0),
    next_fetch_block_(-1),
    log_max_page_id_(log_file_size_ / DATA_PAGE_SIZE),
    finished_(false),
    next_lsn_(LOG_START_LSN),
    log_file_path_("/home/lemon/mysql/data/ib_logfile0"),
    log_stream_(log_file_path_, std::ios::in | std::ios::binary),
    summary_ofs_(),
    table_ofs_(),
    save_logs_(save_logs)
{
  // 1.填充meta_data_buf
  log_stream_.read(reinterpret_cast<char *>(meta_data_buf_), meta_data_buf_size_);

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
  // 打开日志文件
  if (save_logs_) {
    summary_ofs_.open("/home/lemon/mysql/parsed_logs/log_summary.txt");
  }
}

ApplySystem::~ApplySystem() {
  delete[] parse_buf_;
  parse_buf_ = nullptr;
  delete[] meta_data_buf_;
  meta_data_buf_ = nullptr;
}

bool ApplySystem::PopulateHashMap() {

  auto t1 = std::chrono::steady_clock::now();

  if (next_fetch_page_id_ > log_max_page_id_) {
    std::cerr << "we have processed all redo log."
              << std::endl;
    return false;
  }

  unsigned char buf[DATA_PAGE_SIZE];

  if (!save_logs_) {
    hash_map_.clear();
  }

  // 1.填充parse buffer
  uint32_t end_page_id = next_fetch_page_id_ + (parse_buf_size_ - parse_buf_content_size_) / DATA_PAGE_SIZE;
  for (; next_fetch_page_id_ < end_page_id; ++next_fetch_page_id_) {
//    std::cout << "next_fetch_page_id_:" << next_fetch_page_id_ << std::endl;
    auto t2 = std::chrono::steady_clock::now();
    log_stream_.seekg(static_cast<std::streamoff>(next_fetch_page_id_ * DATA_PAGE_SIZE));
    // 读一个Page放到buf里面去
    log_stream_.read(reinterpret_cast<char *>(buf), DATA_PAGE_SIZE);
    auto t3 = std::chrono::steady_clock::now();
    read_file_time_in_parse += (t3 - t2).count();

    for (int block = 0; block < static_cast<int>((DATA_PAGE_SIZE / LOG_BLOCK_SIZE)); ++block) {
//      std::cout << "block:" << block << std::endl;
      if (next_fetch_page_id_ == 0 && block < 4) continue; // 跳过前面4个block
      auto hdr_no = ~LOG_BLOCK_FLUSH_BIT_MASK & mach_read_from_4(buf + block * LOG_BLOCK_SIZE);
      auto data_len = mach_read_from_2(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_HDR_DATA_LEN);
      auto first_rec = mach_read_from_2(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_FIRST_REC_GROUP);
      auto checkpoint_no = mach_read_from_4(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_CHECKPOINT_NO);
      auto checksum = mach_read_from_4(buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_CHECKSUM);
      // 这个block还没装满，不断轮询这个page的这个block，直到它被装满
      if (data_len != 512) {
        std::cout << "waiting for (page_id = "
                  << next_fetch_page_id_ << ", block = "
                  << block << ") be filled." << std::endl;
        std::cout << "logs_applied: " << logs_applied << std::endl;
        std::cout << "read_file_time_in_parse: " << read_file_time_in_parse << std::endl;
        std::cout << "read_file_time_in_apply: " << read_file_time_in_apply << std::endl;
        std::cout << "parse_time: " << parse_time << std::endl;
        std::cout << "parse_body_time: " << parse_body_time << std::endl;
        std::cout << "apply_time: " << apply_time << std::endl;
        std::cout << "total_time: " << total_time << std::endl;
        std::cout << "parse_file_len: " << parse_file_len << std::endl;
        std::cout << "apply_file_len: " << apply_file_len << std::endl;
        exit(0);
        SaveLogs();
        unsigned char tmp_buf[DATA_PAGE_SIZE];
        while (data_len != 512) {
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(1s);

          log_stream_.seekg(static_cast<std::streamoff>(next_fetch_page_id_ * DATA_PAGE_SIZE));
          log_stream_.read(reinterpret_cast<char *>(tmp_buf), DATA_PAGE_SIZE);
          data_len = mach_read_from_2(tmp_buf + block * LOG_BLOCK_SIZE + LOG_BLOCK_HDR_DATA_LEN);
        }
        std::cout << "(page_id = "
                  << next_fetch_page_id_ << ", block = "
                  << block << ") was filled." << std::endl;
      }
      // 每个block的日志掐头去尾放到parse buffer中
      uint32_t len = data_len - LOG_BLOCK_HDR_SIZE - LOG_BLOCK_TRL_SIZE;
//      std::cerr << "hdr_no = " << hdr_no << ", data_len = " << data_len << ", "
//                << "first_rec = " << first_rec << ", checkpoint_no = " << checkpoint_no << ", "
//                << "checksum = " << checksum << std::endl;

      std::memcpy(parse_buf_ptr_ + parse_buf_content_size_,
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
    auto t4 = std::chrono::steady_clock::now();
    len = ParseSingleLogRecord(type, start_ptr, end_ptr, space_id, page_id, &log_body_ptr);
    auto t5 = std::chrono::steady_clock::now();
    parse_time += (t5 - t4).count();
    if (len == 0) {
      break;
    }
    // 加入哈希表
    hash_map_[space_id][page_id].emplace_back(type, space_id, page_id,
                                              next_lsn_, len, log_body_ptr,
                                              start_ptr + len);


    if (save_logs_) {
      summary_ofs_ << "lsn = " << next_lsn_ << ", type = " << GetLogString(type)
                   << ", space_id = " << space_id << ", page_id = "
                   << page_id << ", data_len = " << len << std::endl;
    }


    start_ptr += len;
    parse_file_len += len;
    next_lsn_ = recv_calc_lsn_on_data_add(next_lsn_, len);
  }


  // 把没有解析完成的日志移动到另一个缓冲区
  parse_buf_content_size_ = end_ptr - start_ptr;
  // 双buffer切换
  parse_buf_ptr_ = parse_buf_ + (parse_buf_ptr_ - parse_buf_ + parse_buf_size_) % (parse_buf_size_ * 2);
  std::memcpy(parse_buf_ptr_, start_ptr, parse_buf_content_size_);

  auto t4 = std::chrono::steady_clock::now();
  total_time += (t4 - t1).count();
  parse_time += (t4 - t1).count();
  return true;
}
bool ApplySystem::ApplyHashLogs() {
  auto t1 = std::chrono::steady_clock::now();
  if (hash_map_.empty()) return false;
  for (const auto &spaces_logs: hash_map_) {

    auto space_id = spaces_logs.first;

    if (!(space_id >= 23 && space_id <= 42)) {
      continue;
    }

    for (const auto &pages_logs: spaces_logs.second) {

      auto page_id = pages_logs.first;

//      if (!(space_id == 40 && page_id == 36)) {
//        continue;
//      }

      // 获取需要的page
      auto t2 = std::chrono::steady_clock::now();
      Page *page = buffer_pool.GetPage(space_id, page_id);
      auto t3 = std::chrono::steady_clock::now();
      read_file_time_in_apply += (t3 - t2).count();

      if (page == nullptr) continue;

      lsn_t page_lsn = page->GetLSN();
//      std::cout << "space_id = " << page->GetSpaceId() << ", page_id = " << page->GetPageId() << ", page_lsn = " << page_lsn << std::endl;

      for (const auto &log: pages_logs.second) {
        lsn_t log_lsn = log.log_start_lsn_;
//        if (log_lsn == 101831414) {
//          int x = 0;
//        }
//        if (log.type_ == MLOG_INIT_FILE_PAGE2) {
//          can_apply_[space_id][page_id] = true;
//        } else if (log.type_ == MLOG_COMP_REC_DELETE) {
//          can_apply_[space_id][page_id] = false;
//        } else if (log.type_ == MLOG_COMP_LIST_START_DELETE) {
//          can_apply_[space_id][page_id] = false;
//        } else if (log.type_ == MLOG_INDEX_LOAD) {
//          can_apply_[space_id][page_id] = false;
//        } else if (log.type_ == MLOG_COMP_LIST_END_DELETE) {
//          can_apply_[space_id][page_id] = false;
//        } else {
//
//        }
//        if (!can_apply_[space_id][page_id]) {
//          continue;
//        }
        if (log_lsn <= checkpoint_lsn_) {
          continue;
        }
        // skip!
        if (page_lsn > log_lsn) {
//          std::cout << "This page(space_id = " << space_id << ", page_id = "
//                    << page_id << ", lsn = " << page_lsn <<") is newer than log, skip apply phase." << std::endl;
          continue;
        }

//        if (log.type_ == MLOG_MULTI_REC_END) {
//          continue;
//        }
//        std::cout << "Appling log(lsn = " << log_lsn <<  ", type = " << GetLogString(log.type_) << ", space_id = "
//                  << log.space_id_ << ", page_id = " << log.page_id_ <<") to page." << std::endl;
        auto t4 = std::chrono::steady_clock::now();

        if (ApplyOneLog(page, log)) {
          apply_file_len += log.log_len_;
          page->WritePageLSN(log_lsn + log.log_len_);
          page->WriteCheckSum(BUF_NO_CHECKSUM_MAGIC);
          auto t5 = std::chrono::steady_clock::now();
          apply_time += (t5 - t4).count();
//          static std::ofstream ofs("/home/lemon/mysql/debug_data/redo_applier_debug", std::ios::binary | std::ios::out);
//          ofs.write((const char *) page->GetData(), DATA_PAGE_SIZE);
//          int fd = open("/home/lemon/fifo_redo_applier", O_WRONLY);
//          if (fd == -1) {
//            std::cout << "open fifo_redo_applier failed." << std::endl;
//            exit(1);
//          }
//          long written = 0;
//          while (written != DATA_PAGE_SIZE) {
//            long ret = write(fd, page->GetData() + written, DATA_PAGE_SIZE - written);
//            if (ret == -1) {
//              std::cout << "write to fifo_redo_applier failed." << std::endl;
//              exit(1);
//            }
//            written += ret;
//          }
//          close(fd);
//          std::cout << "Success." << std::endl;
          logs_applied++;
        } else {
//          std::cout << "Skip." << std::endl;
        }

//        if (log_lsn == 31203747) {
//          buffer_pool.WriteBack(space_id, page_id);
//        }
//        buffer_pool.WriteBack(space_id, page_id);

//        ofs << "type = " << GetLogString(log.index_type_)
//            << ", space_id = " << log.space_id_ << ", page_id = "
//            << log.page_id_ << ", data_len = " << log.log_body_len_ << ", lsn = " << log.log_start_lsn_ << std::endl;
      }
    }
  }
  auto t6 = std::chrono::steady_clock::now();
  total_time += (t6 - t1).count();
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

void ApplySystem::SaveLogs() {
  static std::unordered_map<std::string, int> open_times; // 记录文件被打开的次数
  if (save_logs_) {
    for (const auto &spaces_logs: hash_map_) {
      // 打开文件
      space_id_t space_id = spaces_logs.first;
      std::string output_file_name("/home/lemon/mysql/parsed_logs/");
      std::string table_name = buffer_pool.GetFilename(space_id);
      table_name = table_name.substr(table_name.rfind('/') + 1);
      table_name = table_name.substr(0, table_name.size() - 4);
      if (table_name.empty()) {
        continue;
      }
      output_file_name += table_name;
      output_file_name += "_log.txt";
      if (open_times[output_file_name] == 0) {
        table_ofs_.open(output_file_name, std::ios::out | std::ios::trunc);
      } else {
        table_ofs_.open(output_file_name, std::ios::out | std::ios::app);
      }
      open_times[output_file_name]++;
      for (const auto &pages_logs: spaces_logs.second) {
        page_id_t page_id = pages_logs.first;
        for (const auto &log: pages_logs.second) {
          table_ofs_ << "lsn = " << log.log_start_lsn_ << ", type = " << GetLogString(log.type_)
                     << ", space_id = " << space_id << ", page_id = "
                     << page_id << ", data_len = " << log.log_len_ << std::endl;
        }
      }
      // 关闭文件
      table_ofs_.close();
    }
  }
}

}

