#pragma once
#include "config.h"
#include "bean.h"
#include "stl/hash_map.hpp"
#include "stl/std_compat.hpp"
#include "stl/list.hpp"
#include "buffer_pool.h"
namespace Lemon {

class ApplySystem {
public:
  ApplySystem();
  ~ApplySystem();
  lsn_t GetCheckpointLSN() const {
    return checkpoint_lsn_;
  }
  uint32_t GetCheckpointNo() const {
    return checkpoint_no_;
  }
  uint32_t GetCheckpointOffset() const {
    return checkpoint_offset_;
  }
  bool PopulateHashMap();
  bool ApplyHashLogs();

  void SetBufferPool(BufferPool *buffer_pool) {
    buffer_pool_ = buffer_pool;
  }
  static bool ApplyOneLog(Page *page, const LogEntry &log);

private:

  using Key1Type = space_id_t;
  using Key2Type = page_id_t;
  using ValueType = frg::list<LogEntry, frg::stl_allocator>;
  using Key2ValueType = frg::hash_map<Key2Type, ValueType, frg::hash<Key2Type>, frg::stl_allocator>;

//  frg::hash<Key1Type> key1_hasher_;
//  frg::hash<Key2Type> key2_hasher_;

  // 在恢复page时使用的哈希表
  frg::hash_map<Key1Type, Key2ValueType, frg::hash<Key1Type>, frg::stl_allocator>
      hash_map_{frg::hash<Key1Type>{}};

  // parse buffer size in bytes
  uint32_t parse_buf_size_ {2 * 1024 * 1024}; // 2M

  // 左半边与右半边组成一个双buffer，防止某一个buffer中的log还没有被apply就被覆盖了
  byte *parse_buf_ {(byte *) mmap(nullptr,
                                  parse_buf_size_ * 2,
                                  PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG,
                                  -1,
                                  0)};

  // 存放log block中掐头去尾后的redo日志，这些日志必须是完整的MTR，指向双buffer中的某一个
  unsigned char *parse_buf_ptr_ {parse_buf_};

  // parse buffer中有效日志的长度
  uint32_t parse_buf_content_size_ {0};

  // meta buffer size in bytes
  uint32_t meta_data_buf_size_ {DATA_PAGE_SIZE}; // 这个值不能给小了，flash 最小要读1个page上来

  // 存储redo log file的前4个字节
  byte *meta_data_buf_ {(byte *) mmap(nullptr,
                                               meta_data_buf_size_,
                                               PROT_READ | PROT_WRITE,
                                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG,
                                               -1,
                                               0)};

  byte *page_buf_ = (byte *) mmap(nullptr,
                            DATA_PAGE_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG,
                            -1,
                            0);

  lsn_t checkpoint_lsn_ {0};

  uint32_t checkpoint_no_ {0};

  uint32_t checkpoint_offset_ {0};

  uint64_t log_file_size_ {1 * 1024 * 1024 * 1024}; // 1G

  // 下次取出log file中这个log block no代表的block到log_buf_中
  uint32_t next_fetch_page_id_ {LOG_PARTITION_START_LPA};
  // 如果这个值不为-1，说明某一次解析日志时，page内有剩余没有解析完成的日志，下次需要把这个page解析完成
  int next_fetch_block_{-1};

  uint32_t log_max_page_id_ {static_cast<uint32_t>((PARTITION_SIZE * LOG_PARTITION + log_file_size_) / DATA_PAGE_SIZE)};

  // 产生的所有日志都已经apply完了
  bool finished_ {false};

  // 下一条日志的LSN
  lsn_t next_lsn_ {LOG_START_LSN};

  BufferPool *buffer_pool_ {};
};

}
