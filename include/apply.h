#pragma once
#include "config.h"
#include "bean.h"
#include <unordered_map>
#include <list>
#include <string>
#include <fstream>
#include "buffer_pool.h"
namespace Lemon {

class ApplySystem {
public:
  ApplySystem(bool save_logs);
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

  static bool ApplyOneLog(Page *page, const LogEntry &log);

  void SetSaveLogs(bool save) {
    save_logs_ = save;
  }

  void SaveLogs();
private:
  // 在恢复page时使用的哈希表
  std::unordered_map<space_id_t, std::unordered_map<page_id_t, std::list<LogEntry>>> hash_map_;

  // parse buffer size in bytes
  uint32_t parse_buf_size_;

  // 左半边与右半边组成一个双buffer，防止某一个buffer中的log还没有被apply就被覆盖了
  unsigned char *parse_buf_;

  // 存放log block中掐头去尾后的redo日志，这些日志必须是完整的MTR，指向双buffer中的某一个
  unsigned char *parse_buf_ptr_;

  // parse buffer中有效日志的长度
  uint32_t parse_buf_content_size_;

  // meta buffer size in bytes
  uint32_t meta_data_buf_size_;

  // 存储redo log file的前4个字节
  unsigned char *meta_data_buf_;

  lsn_t checkpoint_lsn_;

  uint32_t checkpoint_no_;

  uint32_t checkpoint_offset_;

  uint64_t log_file_size_;

  // 下次取出log file中这个log block no代表的block到log_buf_中
  uint32_t next_fetch_page_id_;
  // 如果这个值不为-1，说明某一次解析日志时，page内有剩余没有解析完成的日志，下次需要把这个page解析完成
  int next_fetch_block_;

  uint32_t log_max_page_id_;
  // 产生的所有日志都已经apply完了
  bool finished_;

  // 下一条日志的LSN
  lsn_t next_lsn_;

  // redo log file path
  std::string log_file_path_;

  // 用来读取log文件的流
  std::ifstream log_stream_;

  std::ofstream summary_ofs_; // 用以保存解析出来的汇总log文件的stream

  std::ofstream table_ofs_; // 分别对每张表保存其log文件
  bool save_logs_;

  // 遇到MLOG_INIT_FILE_PAGE2类型的日志，才可以apply
  std::unordered_map<space_id_t, std::unordered_map<page_id_t, bool>> can_apply_{};
};

}
