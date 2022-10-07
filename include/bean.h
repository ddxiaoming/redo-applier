#pragma once
#include "config.h"
namespace Lemon {

// 一条redo log
class LogEntry {
public:
  LogEntry(LOG_TYPE type, space_id_t space_id,
           page_id_t page_id, lsn_t lsn, size_t log_body_len,
           byte *log_body_start_ptr, byte *log_body_end_ptr) :
      type_(type), space_id_(space_id), page_id_(page_id), lsn_(lsn), log_body_len_(log_body_len),
      log_body_start_ptr_(log_body_start_ptr), log_body_end_ptr_(log_body_end_ptr)
  {}

  LOG_TYPE type_;
  space_id_t space_id_;
  page_id_t page_id_;
  lsn_t lsn_;
  size_t log_body_len_;
  byte *log_body_start_ptr_; // 闭区间
  byte *log_body_end_ptr_; // 开区间
};


}
