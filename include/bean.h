#pragma once
#include "config.h"
#include <vector>
#include <memory>
#include <cassert>
namespace Lemon {

// 一条redo log
class LogEntry {
public:
  LogEntry(LOG_TYPE type, space_id_t space_id,
           page_id_t page_id, lsn_t lsn, size_t log_len,
           byte *log_body_start_ptr, byte *log_body_end_ptr) :
      type_(type), space_id_(space_id), page_id_(page_id), log_start_lsn_(lsn), log_len_(log_len),
      log_body_start_ptr_(log_body_start_ptr), log_body_end_ptr_(log_body_end_ptr)
  {}

  LOG_TYPE type_;
  space_id_t space_id_;
  page_id_t page_id_;
  lsn_t log_start_lsn_;
  size_t log_len_; // 整条redo log的长度（包括log body和log header）
  byte *log_body_start_ptr_; // 闭区间 log body的起始地址
  byte *log_body_end_ptr_; // 开区间 log body的结束地址
};
class RecordInfo;


class FieldInfo {
public:
  friend class RecordInfo;
  FieldInfo() = default;
  FieldInfo(uint32_t main_type, uint32_t precise_type, uint32_t length, uint32_t fixed_length) :
  main_type_(main_type), precise_type_(precise_type), length_(length), fixed_length_(fixed_length) {}

public:
//  bool is_fixed_; // true if fixed-length, false if variable-length

//  bool is_not_null_; // 这一列是否定义为not null

  uint32_t main_type_;
  uint32_t precise_type_;
  uint32_t length_;
  uint32_t fixed_length_;
};

class RecordInfo {
public:
  inline void SetNFields(uint32_t n_fields) {
    n_fields_ = n_fields;
  }
  inline void SetRecPtr(byte *rec_ptr) {
    rec_ptr_ = rec_ptr;
  }
  inline void SetNUnique(uint32_t n_unique) {
    n_unique_ = n_unique;
  }
  inline void SetIndexType(uint32_t index_type) {
    index_type_ = index_type;
  }
  void AddField(uint32_t main_type, uint32_t precise_type, uint32_t length);


  void CalculateOffsets(uint32_t max_n);

  void InitOffsetsCompOrdinary();


  uint32_t GetExtraSize() const;

  uint32_t GetDataSize() const;

  byte *GetRecPtr() const {
    return rec_ptr_;
  }

  uint32_t GetNOffset(uint32_t n) const;
private:

  byte *rec_ptr_ = nullptr; // 这条record的地址
  uint32_t n_fields_{}; // 有多少列，包括系统的隐藏列
  uint32_t n_unique_{};
  uint32_t n_nullable_{}; // 有多少列可以为null
  uint32_t index_type_{}; // index type
  std::vector<FieldInfo> fields_;
  std::vector<uint32_t> offsets_; // 每一个column的偏移量
};


}
