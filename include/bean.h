#pragma once
#include "config.h"
#include <vector>
#include <memory>
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

/** Data structure for a column in a table */
class ColumnInfo {
public:
  /*----------------------*/
  /** The following are copied from dtype_t,
  so that all bit-fields can be packed tightly. */
  /* @{ */
  unsigned	prtype:32;	/*!< precise type; MySQL data
					type, charset code, flags to
					indicate nullability,
					signedness, whether this is a
					binary string, whether this is
					a true VARCHAR where MySQL
					uses 2 bytes to store the length */
  unsigned	mtype:8;	/*!< main data type */

  /* the remaining fields do not affect alphabetical ordering: */

  unsigned	len:16;		/*!< length; for MySQL data this
					is field->pack_length(),
					except that for a >= 5.0.3
					type true VARCHAR this is the
					maximum byte length of the
					string data (in addition to
					the string, MySQL uses 1 or 2
					bytes to store the string length) */

  unsigned	mbminmaxlen:5;	/*!< minimum and maximum length of a
					character, in bytes;
					DATA_MBMINMAXLEN(mbminlen,mbmaxlen);
					mbminlen=DATA_MBMINLEN(mbminmaxlen);
					mbmaxlen=DATA_MBMINLEN(mbminmaxlen) */
  /*----------------------*/
  /* End of definitions copied from dtype_t */
  /* @} */

  unsigned	ind:10;		/*!< table column position
					(starting from 0) */
  unsigned	ord_part:1;	/*!< nonzero if this column
					appears in the ordering fields
					of an index */
  unsigned	max_prefix:12;	/*!< maximum index prefix length on
					this column. Our current max limit is
					3072 for Barracuda table */
};

class TableInfo {
public:

  void AddColumn(uint32_t mtype, uint32_t prtype, uint32_t len);

  // Number of total columns (include system and non-virtual).
  uint32_t n_total_cols_{};

  // Number of system columns.
  uint32_t n_sys_cols{};
  // An array to store columns.
  std::vector<ColumnInfo> cols_{};
private:
};

void TableInfo::AddColumn(uint32_t mtype, uint32_t prtype, uint32_t len) {
  ColumnInfo column{};
  column.len = len;
  column.prtype = prtype;
  column.mtype = mtype;
  column.ind = cols_.size();
  cols_.push_back(column);
}

class IndexInfo {
public:
  uint32_t n_fields_{}; // 有多少列
  uint32_t n_unique_{};
  uint32_t n_nullable_{}; // 有多少列可以为null
  uint32_t type_{}; // index type
  TableInfo table_{}; // 所属的表信息
private:

};
}
