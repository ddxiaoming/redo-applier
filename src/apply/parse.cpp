#include "parse.h"
#include "utility.h"
#include "config.h"
#include "buffer_pool.h"
#include "bean.h"
#include "record.h"
#include <cassert>
#include <cstring>
#include <iostream>

namespace Lemon {

/**
Sets the file page type.
*/
static void fil_page_set_type(byte*	page, uint32_t type) {
  mach_write_to_2(page + FIL_PAGE_TYPE, type);
}

/** Parse a MLOG_FILE_* record.
@param[in]	ptr		redo log record
@param[in]	end		end of the redo log buffer
@param[in]	space_id	the tablespace ID
@param[in]	first_page_no	first page number in the file
@param[in]	type		MLOG_FILE_NAME or MLOG_FILE_DELETE or MLOG_FILE_CREATE2 or MLOG_FILE_RENAME2
@return pointer to next redo log record
@retval nullptr if this log record was truncated */
static byte* PARSE_MLOG_FILE_X(byte* ptr,
                               const byte* end,
                               space_id_t space_id,
                               page_id_t first_page_id,
                               LOG_TYPE	type) {
  // 跳过4个bytes的flag
  if (type == MLOG_FILE_CREATE2) {
    if (end < ptr + 4) {
      return nullptr;
    }
    ptr += 4;
  }

  if (end < ptr + 2) {
    return nullptr;
  }

  // 解析出file name length
  uint16_t len = mach_read_from_2(ptr);
  ptr += 2;
  if (end < ptr + len) {
    return nullptr;
  }

  byte*	end_ptr	= ptr + len;

  switch (type) {
    case MLOG_FILE_NAME:
      break;
    case MLOG_FILE_DELETE:
      break;
    case MLOG_FILE_CREATE2:
      break;
    case MLOG_FILE_RENAME2:
      byte*	new_name = end_ptr + 2;
      if (end < new_name) {
        return nullptr;
      }

      uint16_t new_len = mach_read_from_2(end_ptr);

      if (end < end_ptr + 2 + new_len) {
        return nullptr;
      }

      end_ptr += 2 + new_len;

  }

  return end_ptr;
}

/** Parse a MLOG_TRUNCATE.
@param[in,out]	start_ptr	buffer containing log body to parse
@param[in]	end_ptr		buffer end
@param[in]	space_id	tablespace identifier
@return parsed up to or nullptr. */
static byte* PARSE_MLOG_TRUNCATE(byte* start_ptr, const byte*	end_ptr, space_id_t space_id) {
  if (end_ptr < (start_ptr + 8)) {
    return nullptr;
  }
  start_ptr += 8;
  return start_ptr;
}

/**
Parses a MLOG_*BYTES log record.
@return parsed record end, nullptr if not a complete record or a corrupt record */
byte* ParseOrApplyNBytes(LOG_TYPE type, const byte* log_body_start_ptr, const byte*	log_body_end_ptr, byte *page) {
  uint16_t offset;
  uint32_t val;
  assert(type <= MLOG_8BYTES);
  if (log_body_end_ptr < log_body_start_ptr + 2) {
    return nullptr;
  }

  // 读出偏移量
  offset = mach_read_from_2(log_body_start_ptr);
  log_body_start_ptr += 2;

  if (offset >= DATA_PAGE_SIZE) {
    return nullptr;
  }

  if (type == MLOG_8BYTES) {
    auto dval = mach_u64_parse_compressed(&log_body_start_ptr, log_body_end_ptr);
    if (log_body_start_ptr == nullptr) {
      return nullptr;
    }
    // 对MLOG_8BYTES类型的日志进行Apply
    if (page) {
      mach_write_to_8(page + offset, dval);
    }
    return const_cast<byte*>(log_body_start_ptr);
  }

  // 读出value
  val = mach_parse_compressed(&log_body_start_ptr, log_body_end_ptr);

  if (log_body_start_ptr == nullptr) {
    return nullptr;
  }

  switch (type) {
    case MLOG_1BYTE:
      // 日志损坏
      if (val > 0xFFUL) {
        log_body_start_ptr = nullptr;
      }
      // 对MLOG_1BYTES类型的日志进行Apply
      if (page) {
        mach_write_to_1(page + offset, val);
      }
      break;
    case MLOG_2BYTES:
      // 日志损坏
      if (val > 0xFFFFUL) {
        log_body_start_ptr = nullptr;
      }
      // 对MLOG_2BYTES类型的日志进行Apply
      if (page) {
        mach_write_to_2(page + offset, val);
      }
      break;
    case MLOG_4BYTES:
      // 对MLOG_4BYTES类型的日志进行Apply
      if (page) {
        mach_write_to_2(page + offset, val);
      }
    default:
      break;
  }

  return const_cast<byte*>(log_body_start_ptr);
}

/********************************************************//**
Parses a log record written by mlog_open_and_write_index.
@return parsed record end, nullptr if not a complete record */
static byte* mlog_parse_index(
    byte*		ptr,	/*!< in: buffer */
    const byte*	end_ptr,/*!< in: buffer end */
    bool comp	/*!< in: TRUE=compact row format */) {
  uint32_t n;
  if (comp) {
    if (end_ptr < ptr + 4) {
      return nullptr;
    }
    n = mach_read_from_2(ptr);
    ptr += 2;
    ptr += 2;
    if (end_ptr < ptr + n * 2) {
      return nullptr;
    }
  }
  if (comp) {
    ptr += n * 2;
  }
  return(ptr);
}

/**
Parses the log data of system field values.
@return log data end or NULL */
static byte*
row_upd_parse_sys_vals(
    const byte*	ptr,	/*!< in: buffer */
    const byte*	end_ptr/*!< in: buffer end */)
{
  mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return nullptr;
  }

  if (end_ptr < ptr + DATA_ROLL_PTR_LEN) {
    return nullptr;
  }

  ptr += DATA_ROLL_PTR_LEN;

  mach_u64_parse_compressed(&ptr, end_ptr);

  return(const_cast<byte*>(ptr));
}

/**
Parses a MLOG_REC_SEC_DELETE_MARK or MLOG_COMP_REC_SEC_DELETE_MARK redo log record.
@return end of log record or NULL */
static byte*PARSE_MLOG_REC_SEC_DELETE_MARK(byte* ptr, const byte* end_ptr) {
  if (end_ptr < ptr + 3) {
    return nullptr;
  }
  ptr++;
  uint16_t offset = mach_read_from_2(ptr);
  ptr += 2;
  assert(offset <= DATA_PAGE_SIZE);
  return(ptr);
}

/*********************************************************************//**
Parses the log data written by row_upd_index_write_log.
@return log data end or NULL */
byte*
row_upd_index_parse(const byte*	ptr, const byte* end_ptr, UpdateInfo *update_info) {
  if (end_ptr < ptr + 1) {
    return nullptr;
  }
  uint32_t info_bits = mach_read_from_1(ptr);
  ptr++;
  uint32_t n_fields = mach_parse_compressed(&ptr, end_ptr);
  if (ptr == nullptr) {
    return nullptr;
  }

  if (update_info != nullptr) {
    update_info->n_fields_ = n_fields;
    update_info->info_bits_ = info_bits;
    update_info->fields_.resize(n_fields);
  }

  // 解析出每一个要更新的列
  for (int i = 0; i < n_fields; i++) {
    uint32_t field_no = mach_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }

    if (field_no >= REC_MAX_N_FIELDS && update_info != nullptr) {
      update_info->fields_[i].prtype_ |= DATA_VIRTUAL;
      field_no -= REC_MAX_N_FIELDS;
    }

    if (update_info != nullptr) {
      update_info->fields_[i].field_no_ = field_no;
    }


    uint32_t len = mach_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }

    if (len != UNIV_SQL_NULL) {
      if (end_ptr < ptr + len) {
        return nullptr;
      }
      // 拷贝新值
      if (update_info != nullptr) {
        update_info->fields_[i].CopyData(ptr, len);
      }
      ptr += len;
    } else {
      // 新值是NULL
      if (update_info != nullptr) {
        update_info->fields_[i].ResetData();
      }
    }
  }
  return(const_cast<byte*>(ptr));
}

/**
Parses a PARSE_MLOG_REC_UPDATE_IN_PLACE or PARSE_MLOG_COMP_REC_UPDATE_IN_PLACE redo log record.
@return end of log record or NULL */
byte*
PARSE_MLOG_REC_UPDATE_IN_PLACE(byte* ptr, const byte* end_ptr) {
  if (end_ptr < ptr + 1) {
    return nullptr;
  }
  ptr++;
  ptr = row_upd_parse_sys_vals(ptr, end_ptr);
  if (ptr == nullptr) {
    return nullptr;
  }

  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  uint16_t rec_offset = mach_read_from_2(ptr);
  ptr += 2;

  assert(rec_offset <= DATA_PAGE_SIZE);

  ptr = row_upd_index_parse(ptr, end_ptr, nullptr);

  return(ptr);
}


/**
Parses a MLOG_REC_INSERT or MLOG_REC_COM_INSERT log record.
@return end of log record or nullptr */
static byte*
PARSE_MLOG_REC_INSERT(
    bool 		is_short,/*!< in: TRUE if short inserts */
    const byte*	ptr,	/*!< in: buffer */
    const byte*	end_ptr/*!< in: buffer end */)
{
  uint32_t origin_offset		= 0; /* remove warning */
  uint32_t end_seg_len;
  uint32_t	mismatch_index		= 0; /* remove warning */

  if (!is_short) {
    uint16_t	offset;
    if (end_ptr < ptr + 2) {
      return nullptr;
    }
    // 解析出上一条记录的偏移量
    offset = mach_read_from_2(ptr);
    ptr += 2;
    if (offset >= DATA_PAGE_SIZE) {
      return nullptr;
    }
  }

  end_seg_len = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return nullptr;
  }

  if (end_seg_len >= DATA_PAGE_SIZE << 1) {
    return nullptr;
  }

  if (end_seg_len & 0x1UL) {
    if (end_ptr < ptr + 1) {
      return nullptr;
    }
    ptr++;
    origin_offset = mach_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }
    assert(origin_offset < DATA_PAGE_SIZE);
    mismatch_index = mach_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }
    assert(mismatch_index < DATA_PAGE_SIZE);
  }
  if (end_ptr < ptr + (end_seg_len >> 1)) {
    return nullptr;
  }
  return(const_cast<byte*>(ptr + (end_seg_len >> 1)));
}

/**
Parses a MLOG_REC_CLUST_DELETE_MARK or MLOG_COMP_REC_CLUST_DELETE_MARK redo log record.
@return end of log record or nullptr */
static byte* PARSE_MLOG_REC_CLUST_DELETE_MARK(
    byte*		ptr,	/*!< in: buffer */
    const byte*	end_ptr/*!< in: buffer end */) {

  if (end_ptr < ptr + 2) {
    return nullptr;
  }
  ptr++;
  ptr++;

  ptr = row_upd_parse_sys_vals(ptr, end_ptr);

  if (ptr == nullptr) {
    return nullptr;
  }

  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  uint16_t offset = mach_read_from_2(ptr);
  ptr += 2;

  assert(offset <= DATA_PAGE_SIZE);
  return ptr;
}


/**********************************************************//**
Parses a log record of a record list end or start deletion.
@return end of log record or NULL */
static byte*
PARSE_DELETE_REC_LIST(
    LOG_TYPE type,	/*!< in: MLOG_LIST_END_DELETE,
				MLOG_LIST_START_DELETE,
				MLOG_COMP_LIST_END_DELETE or
				MLOG_COMP_LIST_START_DELETE */
    byte*		ptr,	/*!< in: buffer */
    const byte*		end_ptr/*!< in: buffer end */) {
  assert(type == MLOG_LIST_END_DELETE
        || type == MLOG_LIST_START_DELETE
        || type == MLOG_COMP_LIST_END_DELETE
        || type == MLOG_COMP_LIST_START_DELETE);

  /* Read the record offset as a 2-byte ulint */

  if (end_ptr < ptr + 2) {
    return nullptr;
  }
  ptr += 2;
  return ptr;
}

/**********************************************************//**
Parses a log record of copying a record list end to a new created page.
@return end of log record or NULL */
static byte*
PARSE_COPY_REC_LIST_TO_CREATED_PAGE(byte* ptr, const byte* end_ptr) {
  byte* rec_end;
  uint32_t log_data_len;
  if (ptr + 4 > end_ptr) {
    return nullptr;
  }

  log_data_len = mach_read_from_4(ptr);
  ptr += 4;

  rec_end = ptr + log_data_len;

  if (rec_end > end_ptr) {
    return nullptr;
  }

  return rec_end;
}

/**
Parses a redo log record of reorganizing a page.
@return end of log record or NULL */
byte*
PARSE_PAGE_REORGANIZE(
/*======================*/
    byte*		ptr,	/*!< in: buffer */
    const byte*		end_ptr,/*!< in: buffer end */
    bool		compressed/*!< in: true if compressed page */) {

  if (compressed) {
    if (ptr == end_ptr) {
      return nullptr;
    }

    uint8_t level = mach_read_from_1(ptr);
    assert(level <= 9);
    ++ptr;
  }
  return ptr;
}

/**
MLOG_UNDO_INSERT。既可以解析（page == nullptr），也可以Apply(page != nullptr)。
@return end of log record or nullptr */
static byte* PARSE_OR_APPLY_ADD_UNDO_REC(
    byte*	ptr,	/*!< in: buffer */
    const byte*	end_ptr,/*!< in: buffer end */
    byte*	page)	/*!< in: page or nullptr */
{
  uint16_t len;
  byte*	rec;
  uint16_t first_free;

  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  len = mach_read_from_2(ptr);
  ptr += 2;

  if (end_ptr < ptr + len) {
    return nullptr;
  }

  if (page == nullptr) {
    return(ptr + len);
  }

  first_free = mach_read_from_2(page + TRX_UNDO_PAGE_HDR
                                + TRX_UNDO_PAGE_FREE);
  rec = page + first_free;

  mach_write_to_2(rec, first_free + 4 + len);
  mach_write_to_2(rec + 2 + len, first_free);

  mach_write_to_2(page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
                  first_free + 4 + len);
  std::memcpy(rec + 2, ptr, len);

  return(ptr + len);
}

/**
Parses or apply a redo log record of erasing of an undo page end.
@return end of log record or nullptr */
static byte* PARSE_OR_APPLY_UNDO_ERASE_PAGE_END(byte* ptr, const byte* end_ptr, byte* page) {
  if (page == nullptr) {
    return ptr;
  }

  // 下面是apply的逻辑
  uint16_t first_free = mach_read_from_2(page + TRX_UNDO_PAGE_HDR
                                + TRX_UNDO_PAGE_FREE);
  memset(page + first_free, 0xff,
         (DATA_PAGE_SIZE - FIL_PAGE_DATA_END) - first_free);

  return ptr;
}

/**
Parses the redo log entry of an undo log page initialization.
@return end of log record or NULL */
static byte* PARSE_OR_APPLY_UNDO_PAGE_INIT(const byte*	ptr, const byte* end_ptr, byte*	page) {
  uint32_t type = mach_parse_compressed(&ptr, end_ptr);
  if (ptr == nullptr) {
    return nullptr;
  }

  // 下面是apply的逻辑
  if (page) {
    byte *page_hdr = page + TRX_UNDO_PAGE_HDR;

    mach_write_to_2(page_hdr + TRX_UNDO_PAGE_TYPE, type);

    mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START,
                    TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);
    mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE,
                    TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);

    mach_write_to_2(page + FIL_PAGE_TYPE, FIL_PAGE_UNDO_LOG);
  }

  return const_cast<byte*>(ptr);
}

static byte* ParseOrApplyTrxUndoDiscardLatest(byte* ptr, const byte*	end_ptr, byte* page) {
  if (page) {
    // Apply
    byte *seg_hdr = page + TRX_UNDO_SEG_HDR;
    byte * page_hdr = page + TRX_UNDO_PAGE_HDR;

    uint16_t free = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);
    byte *log_hdr = page + free;

    uint16_t prev_hdr_offset = mach_read_from_2(log_hdr + TRX_UNDO_PREV_LOG);

    if (prev_hdr_offset != 0) {
      byte *prev_log_hdr = page + prev_hdr_offset;

      mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START,
                      mach_read_from_2(prev_log_hdr
                                       + TRX_UNDO_LOG_START));
      mach_write_to_2(prev_log_hdr + TRX_UNDO_NEXT_LOG, 0);
    }

    mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, free);

    mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_CACHED);
    mach_write_to_2(seg_hdr + TRX_UNDO_LAST_LOG, prev_hdr_offset);
  }
  return(ptr);
}

static byte*
ParseOrApplyTrxUndoPageHeader(LOG_TYPE type,
                              const byte* ptr,
                              const byte*	end_ptr,
                              byte *page) {

  trx_id_t	trx_id = mach_u64_parse_compressed(&ptr, end_ptr);

  // 下面是Apply的逻辑
  if (ptr != nullptr && page != nullptr) {
    if (type == MLOG_UNDO_HDR_CREATE) {
      byte *page_hdr = page + TRX_UNDO_PAGE_HDR;
      byte *seg_hdr = page + TRX_UNDO_SEG_HDR;
      uint16_t free = mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE);
      byte *log_hdr = page + free;
      uint32_t new_free = free + TRX_UNDO_LOG_OLD_HDR_SIZE;

      assert(free + TRX_UNDO_LOG_XA_HDR_SIZE < DATA_PAGE_SIZE - 100);

      mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START, new_free);

      mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, new_free);

      mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE);

      uint16_t prev_log = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);

      if (prev_log != 0) {
        byte *prev_log_hdr = page + prev_log;
        mach_write_to_2(prev_log_hdr + TRX_UNDO_NEXT_LOG, free);
      }
      mach_write_to_2(seg_hdr + TRX_UNDO_LAST_LOG, free);

      log_hdr = page + free;

      mach_write_to_2(log_hdr + TRX_UNDO_DEL_MARKS, 1);

      mach_write_to_8(log_hdr + TRX_UNDO_TRX_ID, trx_id);
      mach_write_to_2(log_hdr + TRX_UNDO_LOG_START, new_free);

      mach_write_to_1(log_hdr + TRX_UNDO_XID_EXISTS, 0);
      mach_write_to_1(log_hdr + TRX_UNDO_DICT_TRANS, 0);

      mach_write_to_2(log_hdr + TRX_UNDO_NEXT_LOG, 0);
      mach_write_to_2(log_hdr + TRX_UNDO_PREV_LOG, prev_log);

      return const_cast<byte*>(ptr);
    } else if (type == MLOG_UNDO_HDR_REUSE) {
      byte *page_hdr = page + TRX_UNDO_PAGE_HDR;
      byte *seg_hdr = page + TRX_UNDO_SEG_HDR;

      uint32_t free = TRX_UNDO_SEG_HDR + TRX_UNDO_SEG_HDR_SIZE;

      assert(free + TRX_UNDO_LOG_XA_HDR_SIZE < DATA_PAGE_SIZE - 100);

      byte *log_hdr = page + free;

      uint32_t new_free = free + TRX_UNDO_LOG_OLD_HDR_SIZE;

      /* Insert undo data is not needed after commit: we may free all
      the space on the page */

      assert(mach_read_from_2(page + TRX_UNDO_PAGE_HDR
                            + TRX_UNDO_PAGE_TYPE)
           == TRX_UNDO_INSERT);

      mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START, new_free);

      mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, new_free);

      mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE);

      log_hdr = page + free;

      mach_write_to_8(log_hdr + TRX_UNDO_TRX_ID, trx_id);
      mach_write_to_2(log_hdr + TRX_UNDO_LOG_START, new_free);

      mach_write_to_1(log_hdr + TRX_UNDO_XID_EXISTS, 0);
      mach_write_to_1(log_hdr + TRX_UNDO_DICT_TRANS, 0);
    }
  }

  return const_cast<byte*>(ptr);
}


static byte*
ParseOrApplySetMinRecMark(byte*	ptr, const byte* end_ptr, bool comp, byte*	page) {
  byte*	rec;

  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  // 下面是Apply的逻辑
  if (page) {
    rec = page + mach_read_from_2(ptr);
    if (comp) {
      uint32_t info_bits = rec_get_info_bits(rec, true);
      rec_set_info_bits_new(rec, info_bits | REC_INFO_MIN_REC_FLAG);
    }
  }

  return ptr + 2;
}

static inline byte*
page_dir_get_nth_slot(const byte*	page, uint32_t n) {
  return((byte*)page + DATA_PAGE_SIZE - PAGE_DIR - (n + 1) * PAGE_DIR_SLOT_SIZE);
}

static inline uint32_t page_header_get_field(
    const byte*	page,	/*!< in: page */
    uint32_t 	field)	/*!< in: PAGE_LEVEL, ... */
{
  assert(page);
  assert(field <= PAGE_INDEX_ID);
  return(mach_read_from_2(page + PAGE_HEADER + field));
}

static inline uint32_t
page_dir_get_n_slots(const byte* page) {
  return(page_header_get_field(page, PAGE_N_DIR_SLOTS));
}
static inline uint32_t
rec_get_n_owned_new(const byte*	rec) {
  return(rec_get_bit_field_1(rec, REC_NEW_N_OWNED,
                             REC_N_OWNED_MASK, REC_N_OWNED_SHIFT));
}
/*************************************************************//**
Gets the page number.
@return page number */
static
uint32_t
page_get_page_no(
/*=============*/
    const byte *	page)	/*!< in: page */
{
  assert(page != nullptr);
  return(mach_read_from_4(page + FIL_PAGE_OFFSET));
}
/***************************************************************//**
Looks for the directory slot which owns the given record.
@return the directory slot number */
static uint32_t
page_dir_find_owner_slot(
/*=====================*/
    const byte *page,
    const byte*	rec)	/*!< in: the physical record */
{
  uint16_t 			rec_offs_bytes;
  const byte*	slot;
  const byte*	first_slot;
  const byte*		r = rec;
  // TODO 可能有问题的地方
  first_slot = page_dir_get_nth_slot(page, 0);
  slot = page_dir_get_nth_slot(page, page_dir_get_n_slots(page) - 1);

  while (rec_get_n_owned_new(r) == 0) {
    r = rec_get_next_ptr_const(r);
    assert(r >= page + PAGE_NEW_SUPREMUM);
    assert(r < page + (DATA_PAGE_SIZE - PAGE_DIR));
  }

  rec_offs_bytes = mach_encode_2(r - page);

  while (*(uint16_t *) slot != rec_offs_bytes) {

    if (slot == first_slot) {
      std::cerr << "Probable data corruption on page "
                  << page_get_page_no(page)
                  << ". Original record on that page;";


      std::cerr << "Cannot find the dir slot for this"
                     " record on that page;" << std::endl;
      exit(0);
    }

    slot += PAGE_DIR_SLOT_SIZE;
  }

  return(((uint32_t) (first_slot - slot)) / PAGE_DIR_SLOT_SIZE);
}

static inline void page_header_set_field(byte*	page,	uint32_t field,	uint32_t val) {
  mach_write_to_2(page + PAGE_HEADER + field, val);
}

static inline void page_header_set_ptr(byte* page, uint32_t field, const byte* ptr) {
  uint32_t offs;
  if (ptr == nullptr) {
    offs = 0;
  } else {
    offs = ptr - page;
  }
  page_header_set_field(page, field, offs);
}

static const byte* page_dir_slot_get_rec(const byte *page, const byte *slot) {
  // TODO 可能有问题的地方
  return(static_cast<const byte *>(page + mach_read_from_2(slot)));
}




static byte* ParseOrApplyDeleteRec(byte*	ptr,
                            const byte* end_ptr,
                            byte*	page) {
  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  uint16_t offset = mach_read_from_2(ptr);
  ptr += 2;
  assert(offset <= DATA_PAGE_SIZE);

  // 先不管apply
//  if (page) {
//
//    uint32_t offsets_[100];
//    byte *rec	= page + offset;
//    std::memset(offsets_, 0, 100);
//    offsets_[0] = 100;
//
//    byte* cur_dir_slot;
//    byte* prev_slot;
//    byte* prev_rec = nullptr;
//    byte*	next_rec = nullptr;
//    uint32_t cur_slot_no;
//    uint32_t cur_n_owned;
//
//    byte *current_rec = rec;
//
//
//    /* Save to local variables some data associated with current_rec */
//    cur_slot_no = page_dir_find_owner_slot(page, current_rec);
//    assert(cur_slot_no > 0);
//    cur_dir_slot = page_dir_get_nth_slot(page, cur_slot_no);
//
//    cur_n_owned = rec_get_n_owned_new(current_rec);
//
//    page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);
//
//    prev_slot = page_dir_get_nth_slot(page, cur_slot_no - 1);
//
//    rec = (byte*) page_dir_slot_get_rec(page, prev_slot);
//
//    /* rec now points to the record of the previous directory slot. Look
//    for the immediate predecessor of current_rec in a loop. */
//
//    while (current_rec != rec) {
//      prev_rec = rec;
//      rec = const_cast<byte *>(page_rec_get_next(page, rec));
//    }
//
//    page_cur_move_to_next(cursor);
//    next_rec = cursor->rec;
//
//    /* 3. Remove the record from the linked list of records */
//
//    page_rec_set_next(prev_rec, next_rec);
//
//    /* 4. If the deleted record is pointed to by a dir slot, update the
//    record pointer in slot. In the following if-clause we assume that
//    prev_rec is owned by the same slot, i.e., PAGE_DIR_SLOT_MIN_N_OWNED
//    >= 2. */
//
//
//    ut_ad(cur_n_owned > 1);
//
//    if (current_rec == page_dir_slot_get_rec(cur_dir_slot)) {
//      page_dir_slot_set_rec(cur_dir_slot, prev_rec);
//    }
//
//    /* 5. Update the number of owned records of the slot */
//
//    page_dir_slot_set_n_owned(cur_dir_slot, page_zip, cur_n_owned - 1);
//
//    /* 6. Free the memory occupied by the record */
//    page_mem_free(page, page_zip, current_rec, index, offsets);
//
//    /* 7. Now we have decremented the number of owned records of the slot.
//    If the number drops below PAGE_DIR_SLOT_MIN_N_OWNED, we balance the
//    slots. */
//
//    if (cur_n_owned <= PAGE_DIR_SLOT_MIN_N_OWNED) {
//      page_dir_balance_slot(page, page_zip, cur_slot_no);
//    }
//  }

  return(ptr);
}

static byte* ParseOrApplyIbufBitmapInit(byte* ptr, const byte* end_ptr,byte*	page) {
  if (page) {
    //暂时先不恢复
//    ibuf_bitmap_page_init(block, mtr);
  }

  return(ptr);
}

/**
 * Apply MLOG_COMP_PAGE_CREATE
 */
void ApplyCompPageCreate(byte* page) {
  if (page == nullptr) {
    return;
  }

  // 设置page的种类
  fil_page_set_type(page, FIL_PAGE_INDEX);

  std::memset(page + PAGE_HEADER, 0, PAGE_HEADER_PRIV_END);
  page[PAGE_HEADER + PAGE_N_DIR_SLOTS + 1] = 2; // 初始化PAGE_N_DIR_SLOTS属性
  // // 初始化PAGE_DIRECTION属性

  page[PAGE_HEADER + PAGE_N_HEAP] = 0x80;/*page_is_comp()*/
  page[PAGE_HEADER + PAGE_N_HEAP + 1] = PAGE_HEAP_NO_USER_LOW;
  page[PAGE_HEADER + PAGE_HEAP_TOP + 1] = PAGE_NEW_SUPREMUM_END;
  std::memcpy(page + PAGE_DATA, infimum_supremum_compact,
         sizeof infimum_supremum_compact);
  std::memset(page
         + PAGE_NEW_SUPREMUM_END, 0,
              DATA_PAGE_SIZE - PAGE_DIR - PAGE_NEW_SUPREMUM_END);
  page[DATA_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE * 2 + 1]
      = PAGE_NEW_SUPREMUM;
  page[DATA_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE + 1]
      = PAGE_NEW_INFIMUM;
}

/**
 * Apply MLOG_INIT_FILE_PAGE2
 */
void ApplyInitFilePage2(const LogEntry &log, Page *page) {
  if (page == nullptr) {
    return;
  }
  byte *page_data = page->GetData();
  std::memset(page_data, 0, DATA_PAGE_SIZE);
  // 写入page id
  mach_write_to_4(page_data + FIL_PAGE_OFFSET, log.page_id_);
  // 写入space id
  mach_write_to_4(page_data + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, log.space_id_);
}

/**
 * 从Redo Log中解析出Record信息
 * @param ptr log body start pointer
 * @param end_ptr log body end pointer
 * @param rec_info 索引信息，传出参数
 * @return 该返回值之前的log已经被解析，后续应该从该返回值之后继续解析，如果返回值为nullptr，说明这是一个错误的log格式
 */
static byte* ParseRecInfoFromLog(const byte *ptr, const byte *end_ptr, RecordInfo &rec_info) {

  if (end_ptr < ptr + 4) {
    return nullptr;
  }
  // 解析出要插入的这一个index有多少field
  uint32_t n = mach_read_from_2(ptr);

  ptr += 2;

  // 解析出要插入的这一个index有多少个unique field
  uint32_t n_uniq = mach_read_from_2(ptr);

  ptr += 2;


  assert(n_uniq <= n);
  if (end_ptr < ptr + n * 2) {
    return nullptr;
  }

  // 初始化index的信息
  rec_info.SetNFields(n);
  rec_info.SetNUnique(n_uniq);
  rec_info.SetIndexType(0);

  if (n_uniq != n) {
    assert(n_uniq + DATA_ROLL_PTR <= n);
    rec_info.SetIndexType(DICT_CLUSTERED);
  }

  for (int i = 0; i < n; i++) {
    uint32_t len = mach_read_from_2(ptr);
    ptr += 2;
    /* The high-order bit of len is the NOT NULL flag;
    the rest is 0 or 0x7fff for variable-length fields_,
    and 1..0x7ffe for fixed-length fields_. */
    rec_info.AddField(((len + 1) & 0x7fff) <= 1 ? DATA_BINARY : DATA_FIXBINARY,
                      len & 0x8000 ? DATA_NOT_NULL : 0,
                      len & 0x7fff);
  }
  return const_cast<byte *>(ptr);
}

/*************************************************************//**
Returns the offset stored in the given header field.
@return offset from the start of the page, or 0 */
static inline
uint32_t
page_header_get_offs(
/*=================*/
    const byte*	page,	/*!< in: page */
    uint32_t field)	/*!< in: PAGE_FREE, ... */
{
  uint32_t offs;

  offs = page_header_get_field(page, field);

  return(offs);
}

static byte* page_header_get_ptr(byte *page, uint32_t field) {
  return page_header_get_offs(page, field)
  ? page + page_header_get_offs(page, field) : nullptr;
}

/************************************************************//**
Allocates a block of memory from the free list of an index page. */
static
void
page_mem_alloc_free(
/*================*/
    byte*		page,	/*!< in/out: index page */
    byte*		next_rec,/*!< in: pointer to the new head of the
				free record list */
    uint32_t need)	/*!< in: number of bytes allocated */
{
  uint32_t		garbage;


  page_header_set_ptr(page, PAGE_FREE, next_rec);

  garbage = page_header_get_field(page, PAGE_GARBAGE);

  page_header_set_field(page, PAGE_GARBAGE, garbage - need);
}
/************************************************************//**
Calculates the space reserved for directory slots of a given number of
records. The exact value is a fraction number n * PAGE_DIR_SLOT_SIZE /
PAGE_DIR_SLOT_MIN_N_OWNED, and it is rounded upwards to an integer. */
static
uint32_t
page_dir_calc_reserved_space(
/*=========================*/
    uint32_t	n_recs)		/*!< in: number of records */
{
  return((PAGE_DIR_SLOT_SIZE * n_recs + PAGE_DIR_SLOT_MIN_N_OWNED - 1) / PAGE_DIR_SLOT_MIN_N_OWNED);
}


/*************************************************************//**
Calculates free space if a page is emptied.
@return free space */
static
uint32_t
page_get_free_space_of_empty() {

  return((uint32_t)(DATA_PAGE_SIZE - PAGE_NEW_SUPREMUM_END - PAGE_DIR - 2 * PAGE_DIR_SLOT_SIZE));
}


/*************************************************************//**
Gets the number of records in the heap.
@return number of user records */
static
uint32_t
page_dir_get_n_heap(const byte*	page)	/*!< in: index page */
{
  return(page_header_get_field(page, PAGE_N_HEAP) & 0x7fff);
}
/************************************************************//**
Each user record on a page, and also the deleted user records in the heap
takes its size plus the fraction of the dir cell size /
PAGE_DIR_SLOT_MIN_N_OWNED bytes for it. If the sum of these exceeds the
value of page_get_free_space_of_empty, the insert is impossible, otherwise
it is allowed. This function returns the maximum combined size of records
which can be inserted on top of the record heap.
@return maximum combined size for inserted records */
static
uint32_t
page_get_max_insert_size(
/*=====================*/
    const byte*	page,	/*!< in: index page */
    uint32_t n_recs)	/*!< in: number of records */
{
  uint32_t	occupied;
  uint32_t	free_space;

  occupied = page_header_get_field(page, PAGE_HEAP_TOP)
             - PAGE_NEW_SUPREMUM_END
             + page_dir_calc_reserved_space(
      n_recs + page_dir_get_n_heap(page) - 2);

  free_space = page_get_free_space_of_empty();

  /* Above the 'n_recs +' part reserves directory space for the new
  inserted records; the '- 2' excludes page infimum and supremum
  records */

  if (occupied > free_space) {

    return(0);
  }

  return(free_space - occupied);
}

/*************************************************************//**
Sets the number of records in the heap. */
static
void
page_dir_set_n_heap(
/*================*/
    byte *		page,	/*!< in/out: index page */
    uint32_t 		n_heap)	/*!< in: number of records */
{
  assert(n_heap < 0x8000);
  assert(n_heap == (page_header_get_field(page, PAGE_N_HEAP) & 0x7fff) + 1);

  page_header_set_field(page, PAGE_N_HEAP, n_heap | (0x8000 & page_header_get_field(page, PAGE_N_HEAP)));
}
/************************************************************//**
Allocates a block of memory from the heap of an index page.
@return pointer to start of allocated buffer, or NULL if allocation fails */
static byte*
page_mem_alloc_heap(
/*================*/
    byte*		page,	/*!< in/out: index page */
    uint32_t need,	/*!< in: total number of bytes needed */
    uint32_t*		heap_no)/*!< out: this contains the heap number
				of the allocated record
				if allocation succeeds */
{
  byte*	block;
  uint32_t	avl_space;

  assert(page && heap_no);

  avl_space = page_get_max_insert_size(page, 1);

  if (avl_space >= need) {
    block = page_header_get_ptr(page, PAGE_HEAP_TOP);

    page_header_set_ptr(page, PAGE_HEAP_TOP, block + need);
    *heap_no = page_dir_get_n_heap(page);

    page_dir_set_n_heap(page, 1 + *heap_no);

    return(block);
  }

  return nullptr;
}

/************************************************************//**
Gets the pointer to the next record on the page.
@return pointer to next record */
static
byte*
page_rec_get_next(byte* page, const byte * rec)
{
  uint32_t 		offs;
  // TODO 可能有问题的地方
  offs = rec_get_next_offs(page, rec);

  if (offs >= DATA_PAGE_SIZE) {
    // TODO Error
    std::cerr << "Error" << std::endl;
  } else if (offs == 0) {
    return nullptr;
  }

  return (page + offs);
}


/************************************************************//**
Sets the pointer to the next record on the page. */
static
void
page_rec_set_next(
/*==============*/
    const byte *page,
    byte *		rec,	/*!< in: pointer to record,
				must not be page supremum */
    const byte *	next)	/*!< in: pointer to next record,
				must not be page infimum */
{
  uint32_t 	offs;
  assert(rec != next);

  // TODO 可能有问题的地方
  offs = next != nullptr ? next - page : 0;

  rec_set_next_offs_new(page, rec, offs);
}

/***************************************************************//**
Looks for the record which owns the given record.
@return the owner record */
static byte*
page_rec_find_owner_rec(
/*====================*/
    byte *page,
    byte*	rec)
{

  // TODO 可能有问题的地方
  while (rec_get_n_owned_new(rec) == 0) {
    rec = page_rec_get_next(page, rec);
  }

  return(rec);
}

/***************************************************************//**
Gets the number of records owned by a directory slot.
@return number of records */
static
    uint32_t
page_dir_slot_get_n_owned(
/*======================*/
    const byte *page,
    const byte*	slot)	/*!< in: page directory slot */
{
  // TODO 可能有问题的地方
  const byte*	rec	= page_dir_slot_get_rec(page, slot);
  return(rec_get_n_owned_new(rec));
}

/*************************************************************//**
Sets the number of dir slots in directory. */
static
void
page_dir_set_n_slots(
/*=================*/
    byte *	page,	/*!< in/out: page */
    uint32_t n_slots)/*!< in: number of slots */
{
  page_header_set_field(page, PAGE_N_DIR_SLOTS, n_slots);
}


/**************************************************************//**
Used to add n slots to the directory. Does not set the record pointers
in the added slots or update n_owned values: this is the responsibility
of the caller. */
static
void
page_dir_add_slot(
/*==============*/
    byte* page,	/*!< in/out: the index page */

    uint32_t start)	/*!< in: the slot above which the new slots
				are added */
{
  byte*	slot;
  uint32_t n_slots;

  n_slots = page_dir_get_n_slots(page);

  assert(start < n_slots - 1);

  /* Update the page header */
  page_dir_set_n_slots(page, n_slots + 1);

  /* Move slots up */
  slot = page_dir_get_nth_slot(page, n_slots);
  std::memmove(slot, slot + PAGE_DIR_SLOT_SIZE,
          (n_slots - 1 - start) * PAGE_DIR_SLOT_SIZE);
}

/***************************************************************//**
This is used to set the record offset in a directory slot. */
static
void
page_dir_slot_set_rec(
/*==================*/
    const byte *page,
    byte* slot,	/*!< in: directory slot */
    byte*  rec)	/*!< in: record on the page */
{
  // TODO 可能有问题的地方
  mach_write_to_2(slot, rec - page);
}


/***************************************************************//**
This is used to set the owned records field of a directory slot. */
static
void
page_dir_slot_set_n_owned(
/*======================*/
    const byte *page,
    byte *slot,	/*!< in/out: directory slot */
    uint32_t n)	/*!< in: number of records owned by the slot */
{
  // TODO 可能有问题的地方
  byte*	rec	= (byte*) page_dir_slot_get_rec(page, slot);
  rec_set_n_owned_new(rec, n);
}

/****************************************************************//**
Splits a directory slot which owns too many records. */
void
page_dir_split_slot(
/*================*/
    byte*		page,	/*!< in/out: index page */
    uint32_t slot_no)/*!< in: the directory slot */
{
  byte* rec;
  byte*	new_slot;
  byte*	prev_slot;
  byte*	slot;
  uint32_t i;
  uint32_t n_owned;

  assert(page);
  assert(slot_no > 0);

  slot = page_dir_get_nth_slot(page, slot_no);

  n_owned = page_dir_slot_get_n_owned(page, slot);
  assert(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED + 1);

  /* 1. We loop to find a record approximately in the middle of the
  records owned by the slot. */

  prev_slot = page_dir_get_nth_slot(page, slot_no - 1);
  rec = (byte*) page_dir_slot_get_rec(page, prev_slot);

  for (i = 0; i < n_owned / 2; i++) {
    rec = page_rec_get_next(page, rec);
  }

  assert(n_owned / 2 >= PAGE_DIR_SLOT_MIN_N_OWNED);

  /* 2. We add one directory slot immediately below the slot to be
  split. */

  page_dir_add_slot(page, slot_no - 1);

  /* The added slot is now number slot_no, and the old slot is
  now number slot_no + 1 */

  new_slot = page_dir_get_nth_slot(page, slot_no);
  slot = page_dir_get_nth_slot(page, slot_no + 1);

  /* 3. We store the appropriate values to the new slot. */

  page_dir_slot_set_rec(page, new_slot, rec);
  page_dir_slot_set_n_owned(page, new_slot, n_owned / 2);

  /* 4. Finally, we update the number of records field of the
  original slot */

  page_dir_slot_set_n_owned(page, slot, n_owned - (n_owned / 2));
}

/***********************************************************//**
Inserts a record next to page cursor on an uncompressed page.
Returns pointer to inserted record if succeed, i.e., enough
space available, NULL otherwise. The cursor stays at the same position.
@return pointer to record if succeed, NULL otherwise */
byte*
page_cur_insert_rec_low(
/*====================*/
    byte *page, // 相关的page
    RecordInfo &pre_rec_info,
    byte*	pre_rec,/*!< in: pointer to current record after
				which the new record is inserted 指向中间*/
    RecordInfo &inserted_rec_info,	/*!< in: record descriptor */
    byte*	inserted_rec	/*!< in: pointer to a physical record 指向开头*/
    )
{

  byte* insert_buf;
  uint32_t rec_size;
  byte*	last_insert;	/*!< cursor position at previous
					insert */
  byte* free_rec;	/*!< a free record that was reused,
					or NULL */
  byte* insert_rec;	/*!< inserted record */

  uint32_t heap_no = 0;
  /* 1. Get the size of the physical record in the page */
  rec_size = inserted_rec_info.GetExtraSize() + inserted_rec_info.GetDataSize();


  /* 2. Try to find suitable space from page memory management */
  free_rec = page_header_get_ptr(page, PAGE_FREE);
  if (free_rec) {
    /* Try to allocate from the head of the free list. */
    RecordInfo free_rec_info = inserted_rec_info;
    free_rec_info.SetRecPtr(free_rec);
    free_rec_info.CalculateOffsets(ULINT_UNDEFINED);

    // free链表无法重复利用
    if (free_rec_info.GetExtraSize() + free_rec_info.GetDataSize() < rec_size) {
      goto use_heap;
    }

    // free链表可以重复利用
    insert_buf = free_rec - free_rec_info.GetExtraSize();

    heap_no = rec_get_heap_no_new(free_rec);
    page_mem_alloc_free(page, rec_get_next_ptr(free_rec), rec_size);

  } else {
    use_heap:
    free_rec = nullptr;
    insert_buf = page_mem_alloc_heap(page, rec_size, &heap_no);

    if (insert_buf == nullptr) {
      return nullptr;
    }
  }

  /* 3. Copy the record to page */
  insert_rec = rec_copy(insert_buf, inserted_rec_info);

  /* 4. Insert the record in the linked list of records */
  {
    /* next record after current before the insertion */
    byte *next_rec = page_rec_get_next(page, pre_rec);
    page_rec_set_next(page, insert_rec, next_rec);
    page_rec_set_next(page, pre_rec, insert_rec);
  }

  /* 5. Set the n_owned field in the inserted record to zero,
and set the heap_no field */
  rec_set_n_owned_new(insert_rec, 0);
  rec_set_heap_no_new(insert_rec, heap_no);


  /* 6. Update the last insertion info in page header */
  last_insert = page_header_get_ptr(page, PAGE_LAST_INSERT);
  if (last_insert == nullptr) {
    page_header_set_field(page, PAGE_DIRECTION,
                          PAGE_NO_DIRECTION);
    page_header_set_field(page, PAGE_N_DIRECTION, 0);

  } else if ((last_insert == pre_rec)
             && (page_header_get_field(page, PAGE_DIRECTION)
                 != PAGE_LEFT)) {

    page_header_set_field(page, PAGE_DIRECTION,
                          PAGE_RIGHT);
    page_header_set_field(page, PAGE_N_DIRECTION,
                          page_header_get_field(
                              page, PAGE_N_DIRECTION) + 1);

  } else if ((page_rec_get_next(page, insert_rec) == last_insert)
             && (page_header_get_field(page, PAGE_DIRECTION)
                 != PAGE_RIGHT)) {

    page_header_set_field(page, PAGE_DIRECTION,
                          PAGE_LEFT);
    page_header_set_field(page, PAGE_N_DIRECTION,
                          page_header_get_field(
                              page, PAGE_N_DIRECTION) + 1);
  } else {
    page_header_set_field(page, PAGE_DIRECTION,
                          PAGE_NO_DIRECTION);
    page_header_set_field(page, PAGE_N_DIRECTION, 0);
  }



  /* 7. It remains to update the owner record. */
  {
    byte*	owner_rec	= page_rec_find_owner_rec(page, insert_rec);
    uint32_t	n_owned;
    n_owned = rec_get_n_owned_new(owner_rec);
    rec_set_n_owned_new(owner_rec, n_owned + 1);

    /* 8. Now we have incremented the n_owned field of the owner
    record. If the number exceeds PAGE_DIR_SLOT_MAX_N_OWNED,
    we have to split the corresponding directory slot in two. */

    if (n_owned == PAGE_DIR_SLOT_MAX_N_OWNED) {
      page_dir_split_slot(
          page,
          page_dir_find_owner_slot(page, owner_rec));
    }
  }

  return insert_rec;
}

void ApplyCompRecInsert(const LogEntry &log, Page *page) {
  RecordInfo inserted_rec_info;
  const byte *ptr = log.log_body_start_ptr_;
  const byte *end_ptr = log.log_body_end_ptr_;
  ptr = ParseRecInfoFromLog(ptr, end_ptr, inserted_rec_info);

  uint32_t origin_offset = 0;
  uint32_t mismatch_index = 0;
  byte*	cursor_rec;
  byte	buf1[1024];
  byte*	buf;
  uint32_t info_and_status_bits = 0;

  /* Read the cursor rec offset as a 2-byte ulint */

  assert(end_ptr >= ptr + 2);

  // 解析出上一条记录的页内偏移量
  uint32_t offset = mach_read_from_2(ptr);
  ptr += 2;

  assert(offset < DATA_PAGE_SIZE);
  cursor_rec = page->GetData() + offset; // 上一条记录的位置

  // 解析出end_seg_len属性
  uint32_t end_seg_len = mach_parse_compressed(&ptr, end_ptr);
  assert(ptr != nullptr);

  if (end_seg_len >= DATA_PAGE_SIZE << 1) {
    // 到这说明这条log损坏了
    return;
  }

  if (end_seg_len & 0x1UL) {
    /* Read the info bits */

    if (end_ptr < ptr + 1) {
      return;
    }

    // 解析出info_and_status_bits
    info_and_status_bits = mach_read_from_1(ptr);
    ptr++;

    // 解析出origin_offset
    origin_offset = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == nullptr) {
      return;
    }

    assert(origin_offset < DATA_PAGE_SIZE);

    // 解析出mismatch_index
    mismatch_index = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == nullptr) {
      return;
    }

    assert(mismatch_index < DATA_PAGE_SIZE);
  }

  if (end_ptr < ptr + (end_seg_len >> 1)) {
    return;
  }

  /* Read from the log the inserted index record end segment which
  differs from the cursor record */

  RecordInfo pre_rec_info = inserted_rec_info; // 上一条记录
  pre_rec_info.SetRecPtr(cursor_rec);
  pre_rec_info.CalculateOffsets(ULINT_UNDEFINED);

  if (!(end_seg_len & 0x1UL)) {
    info_and_status_bits = rec_get_info_and_status_bits(cursor_rec);
    origin_offset = pre_rec_info.GetExtraSize();
    mismatch_index = pre_rec_info.GetDataSize() + pre_rec_info.GetExtraSize() - (end_seg_len >> 1);
  }

  end_seg_len >>= 1;

  if (mismatch_index + end_seg_len < sizeof buf1) {
    buf = buf1;
  } else {
    buf = static_cast<byte*>(malloc(mismatch_index + end_seg_len));
  }

  /* Build the inserted record to buf */
  std::memcpy(buf, cursor_rec - pre_rec_info.GetExtraSize(), mismatch_index);
  std::memcpy(buf + mismatch_index, ptr, end_seg_len);

  rec_set_info_and_status_bits(buf + origin_offset,
                               info_and_status_bits);

  inserted_rec_info.SetRecPtr(buf + origin_offset);
  inserted_rec_info.CalculateOffsets(ULINT_UNDEFINED);

  // 插入记录
  page_cur_insert_rec_low(page->GetData(), pre_rec_info, cursor_rec, inserted_rec_info, inserted_rec_info.GetRecPtr());

  if (buf != buf1) {
    free(buf);
  }
}

/*****************************************************************//**
Reads a roll ptr from an index page. In case that the roll ptr size
changes in some future version, this function should be used instead of
mach_read_...
@return roll ptr */
static
roll_ptr_t
trx_read_roll_ptr(
/*==============*/
    const byte*	ptr)	/*!< in: pointer to memory from where to read */
{
  return(mach_read_from_7(ptr));
}


/*********************************************************************//**
Parses the log data of system field values.
@return log data end or NULL */
static byte*
row_upd_parse_sys_vals(
/*===================*/
    const byte*	ptr,	/*!< in: buffer */
    const byte*	end_ptr,/*!< in: buffer end */
    uint32_t*		pos,	/*!< out: TRX_ID position in record */
    trx_id_t*	trx_id,	/*!< out: trx id */
    roll_ptr_t*	roll_ptr)/*!< out: roll ptr */
{
  *pos = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {

    return nullptr;
  }

  if (end_ptr < ptr + DATA_ROLL_PTR_LEN) {

    return nullptr;
  }

  *roll_ptr = trx_read_roll_ptr(ptr);
  ptr += DATA_ROLL_PTR_LEN;

  *trx_id = mach_u64_parse_compressed(&ptr, end_ptr);

  return(const_cast<byte*>(ptr));
}

/******************************************************//**
The following function is used to set the deleted bit of a record. */
static
void
btr_rec_set_deleted_flag(
/*=====================*/
    byte*		rec,	/*!< in/out: physical record */
    uint32_t flag)	/*!< in: nonzero if delete marked */
{
  rec_set_deleted_flag_new(rec, flag);
}

/*****************************************************************//**
Writes a trx id to an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_write_... */
static void
trx_write_trx_id(
/*=============*/
    byte* ptr,	/*!< in: pointer to memory where written */
    trx_id_t	id)	/*!< in: id */
{
  assert(id > 0);
  mach_write_to_6(ptr, id);
}

/*****************************************************************//**
Writes a roll ptr to an index page. In case that the size changes in
some future version, this function should be used instead of
mach_write_... */
static
void
trx_write_roll_ptr(
/*===============*/
    byte*		ptr,		/*!< in: pointer to memory where
					written */
    roll_ptr_t	roll_ptr)	/*!< in: roll ptr */
{
  mach_write_to_7(ptr, roll_ptr);
}

/*********************************************************************//**
Updates the trx id and roll ptr field in a clustered index record in database
recovery. */
void
row_upd_rec_sys_fields_in_recovery(
/*===============================*/
    byte*		rec,	/*!< in/out: record */
    const RecordInfo &rec_info,/*!< in: array returned by rec_get_offsets() */
    uint32_t pos,	/*!< in: TRX_ID position in rec */
    trx_id_t	trx_id,	/*!< in: transaction id */
    roll_ptr_t	roll_ptr)/*!< in: roll ptr of the undo log record */
{
  byte*	field;
  uint32_t len;


  field = rec_get_nth_field(rec, rec_info, pos, &len);
  assert(len == DATA_TRX_ID_LEN);
  trx_write_trx_id(field, trx_id);
  trx_write_roll_ptr(field + DATA_TRX_ID_LEN, roll_ptr);
}

void ApplyCompRecClusterDeleteMark(const LogEntry &log, Page *page) {
  RecordInfo deleted_rec_info;
  const byte *ptr = log.log_body_start_ptr_;
  const byte *end_ptr = log.log_body_end_ptr_;
  ptr = ParseRecInfoFromLog(ptr, end_ptr, deleted_rec_info);

  uint32_t flags;
  uint32_t		val;
  uint32_t		pos;
  trx_id_t	trx_id;
  roll_ptr_t	roll_ptr;
  uint32_t		offset;
  byte *rec;

  if (end_ptr < ptr + 2) {
    return;
  }

  flags = mach_read_from_1(ptr);
  ptr++;
  val = mach_read_from_1(ptr);
  ptr++;

  ptr = row_upd_parse_sys_vals(ptr, end_ptr, &pos, &trx_id, &roll_ptr);

  if (ptr == nullptr) {

    return;
  }

  if (end_ptr < ptr + 2) {

    return;
  }

  offset = mach_read_from_2(ptr);
  ptr += 2;

  assert(offset <= DATA_PAGE_SIZE);

  rec = page->GetData() + offset;

  /* We do not need to reserve search latch, as the page
  is only being recovered, and there cannot be a hash index to
  it. Besides, these fields_ are being updated in place
  and the adaptive hash index does not depend on them. */

  uint32_t deleted_flag = rec_get_deleted_flag(rec);
  std::cout << "before apply, delete mark is " << deleted_flag << std::endl;
  btr_rec_set_deleted_flag(rec, val);

  if (!(flags & BTR_KEEP_SYS_FLAG)) {

    deleted_rec_info.SetRecPtr(rec);
    deleted_rec_info.CalculateOffsets(ULINT_UNDEFINED);
    row_upd_rec_sys_fields_in_recovery(rec, deleted_rec_info, pos, trx_id, roll_ptr);
  }
  deleted_flag = rec_get_deleted_flag(rec);
  std::cout << "after apply, delete mark is " << deleted_flag << std::endl;
}

/***********************************************************//**
Replaces the new column values stored in the update vector to the
record given. No field size changes are allowed. This function is
usually invoked on a clustered rec_info. The only use case for a
secondary rec_info is row_ins_sec_index_entry_by_modify() or its
counterpart in ibuf_insert_to_index_page(). */
void
row_upd_rec_in_place(
/*=================*/
    byte*		rec,	/*!< in/out: record where replaced */
    RecordInfo	&rec_info,	/*!< in: the index the record belongs to */
    const UpdateInfo &update	/*!< in: update vector */
    )
{

  rec_set_info_bits_new(rec, update.info_bits_);

  uint32_t n_fields = update.n_fields_;

  for (int i = 0; i < n_fields; i++) {
    /* No need to update virtual columns for non-virtual rec_info */
    if (((update.fields_[i].prtype_ & DATA_VIRTUAL) == DATA_VIRTUAL)
        && !(rec_info.Type() & DICT_VIRTUAL)) {
      continue;
    }

    rec_set_nth_field(rec, rec_info, update.fields_[i].field_no_,
                      update.fields_[i].data_,
                      update.fields_[i].len_);
  }
}
void ApplyCompRecUpdateInPlace(const LogEntry &log, Page *page) {
  RecordInfo update_rec_info;
  const byte *ptr = log.log_body_start_ptr_;
  const byte *end_ptr = log.log_body_end_ptr_;
  ptr = ParseRecInfoFromLog(ptr, end_ptr, update_rec_info);

  uint32_t flags;
  byte*	rec;
  UpdateInfo update;
  uint32_t pos;
  trx_id_t	trx_id;
  roll_ptr_t	roll_ptr;
  uint32_t		rec_offset;

  if (end_ptr < ptr + 1) {

    return;
  }

  flags = mach_read_from_1(ptr);
  ptr++;

  ptr = row_upd_parse_sys_vals(ptr, end_ptr, &pos, &trx_id, &roll_ptr);

  if (ptr == nullptr) {

    return;
  }

  if (end_ptr < ptr + 2) {

    return;
  }

  // 解析出需要更新的记录的偏移量
  rec_offset = mach_read_from_2(ptr);
  ptr += 2;

  assert(rec_offset <= DATA_PAGE_SIZE);


  ptr = row_upd_index_parse(ptr, end_ptr, &update);

  if (!ptr || !page->GetData()) {

    return;
  }

  // 解析出要更新的record的地址
  rec = page->GetData() + rec_offset;

  update_rec_info.SetRecPtr(rec);
  update_rec_info.CalculateOffsets(ULINT_UNDEFINED);

  if (!(flags & BTR_KEEP_SYS_FLAG)) {
    row_upd_rec_sys_fields_in_recovery(rec, update_rec_info, pos, trx_id, roll_ptr);
  }

  // update
  row_upd_rec_in_place(rec, update_rec_info, update);

}
// Apply MLOG_COMP_REC_SEC_DELETE_MARK log.
void ApplyCompRecSecondDeleteMark(const LogEntry &log, Page *page) {
  RecordInfo deleted_rec_info;
  const byte *ptr = log.log_body_start_ptr_;
  const byte *end_ptr = log.log_body_end_ptr_;
  ptr = ParseRecInfoFromLog(ptr, end_ptr, deleted_rec_info);
  uint32_t val;
  uint32_t	offset;
  byte*	rec;
  if (end_ptr < ptr + 3) {
    return;
  }

  val = mach_read_from_1(ptr);
  ptr++;

  offset = mach_read_from_2(ptr);
  ptr += 2;

  assert(offset <= DATA_PAGE_SIZE);

  rec = page->GetData() + offset;

  btr_rec_set_deleted_flag(rec, val);
}

byte* ParseOrApplyString(byte* log_body_start_ptr, const byte* log_body_end_ptr, byte* page) {
  uint32_t offset;
  uint32_t len;

  if (log_body_end_ptr < log_body_start_ptr + 4) {

    return nullptr;
  }

  offset = mach_read_from_2(log_body_start_ptr);
  log_body_start_ptr += 2;
  len = mach_read_from_2(log_body_start_ptr);
  log_body_start_ptr += 2;

  assert(offset < DATA_PAGE_SIZE);
  assert(len + offset <= DATA_PAGE_SIZE);

  if (log_body_end_ptr < log_body_start_ptr + len) {
    return nullptr;
  }

  // Apply
  if (page) {
    std::memcpy(page + offset, log_body_start_ptr, len);
  }

  return(log_body_start_ptr + len);
}

/** Try to parse a single log record body and also applies it if
specified.
@param[in]	type		redo log entry type
@param[in]	ptr		redo log record body
@param[in]	end_ptr		end of buffer
@param[in]	space_id	tablespace identifier
@param[in]	page_no		page number
@return log record end, nullptr if not a complete record */
static byte* ParseSingleLogRecordBody(LOG_TYPE	type,
                                      byte* ptr,
                                      const byte* end_ptr,
                                      space_id_t space_id,
                                      page_id_t page_id) {


  switch (type) {
    case MLOG_FILE_NAME:
    case MLOG_FILE_DELETE:
    case MLOG_FILE_CREATE2:
    case MLOG_FILE_RENAME2:
      return PARSE_MLOG_FILE_X(ptr, end_ptr, space_id, page_id, type);
    case MLOG_INDEX_LOAD:
      if (end_ptr < ptr + 8) {
        return nullptr;
      }
      return ptr + 8;
    case MLOG_TRUNCATE:
      return PARSE_MLOG_TRUNCATE(ptr, end_ptr, space_id);
    default:
      break;
  }

  const byte*	old_ptr = ptr;

  switch (type) {
    case MLOG_1BYTE:
    case MLOG_2BYTES:
    case MLOG_4BYTES:
    case MLOG_8BYTES:
      ptr = ParseOrApplyNBytes(type, ptr, end_ptr, nullptr);
      break;
    case MLOG_REC_INSERT:
    case MLOG_COMP_REC_INSERT:
      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr,type == MLOG_COMP_REC_INSERT))) {
        ptr = PARSE_MLOG_REC_INSERT(false, ptr, end_ptr);
      }
      break;
    case MLOG_REC_CLUST_DELETE_MARK: case MLOG_COMP_REC_CLUST_DELETE_MARK:
      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr,type == MLOG_COMP_REC_CLUST_DELETE_MARK))) {
        ptr = PARSE_MLOG_REC_CLUST_DELETE_MARK(ptr, end_ptr);
      }
      break;
    case MLOG_COMP_REC_SEC_DELETE_MARK:
      ptr = mlog_parse_index(ptr, end_ptr, true);
      if (!ptr) {
        break;
      }
      /* Fall through */
    case MLOG_REC_SEC_DELETE_MARK:
      ptr = PARSE_MLOG_REC_SEC_DELETE_MARK(ptr, end_ptr);
      break;
    case MLOG_REC_UPDATE_IN_PLACE:
    case MLOG_COMP_REC_UPDATE_IN_PLACE:
      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr,type == MLOG_COMP_REC_UPDATE_IN_PLACE))) {
        ptr = PARSE_MLOG_REC_UPDATE_IN_PLACE(ptr, end_ptr);
      }
      break;
    case MLOG_LIST_END_DELETE:
    case MLOG_COMP_LIST_END_DELETE:
    case MLOG_LIST_START_DELETE:
    case MLOG_COMP_LIST_START_DELETE:
      if (nullptr != (ptr = mlog_parse_index(ptr,
                                             end_ptr,
                                             type == MLOG_COMP_LIST_END_DELETE
                                             || type == MLOG_COMP_LIST_START_DELETE))) {
        ptr = PARSE_DELETE_REC_LIST(type, ptr, end_ptr);
      }
      break;
    case MLOG_LIST_END_COPY_CREATED:
    case MLOG_COMP_LIST_END_COPY_CREATED:
      if (nullptr != (ptr = mlog_parse_index(ptr,
                                             end_ptr,
                                             type == MLOG_COMP_LIST_END_COPY_CREATED))) {
        ptr = PARSE_COPY_REC_LIST_TO_CREATED_PAGE(ptr, end_ptr);
      }
      break;
    case MLOG_PAGE_REORGANIZE:
    case MLOG_COMP_PAGE_REORGANIZE:
    case MLOG_ZIP_PAGE_REORGANIZE:
      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr,type != MLOG_PAGE_REORGANIZE))) {
        ptr = PARSE_PAGE_REORGANIZE(ptr, end_ptr,type == MLOG_ZIP_PAGE_REORGANIZE);
      }
      break;

    // 这几种类型的log没有body，不需要解析
    case MLOG_PAGE_CREATE:
    case MLOG_COMP_PAGE_CREATE:
    case MLOG_PAGE_CREATE_RTREE:
    case MLOG_COMP_PAGE_CREATE_RTREE:
      break;
    case MLOG_UNDO_INSERT:
      ptr = PARSE_OR_APPLY_ADD_UNDO_REC(ptr, end_ptr, nullptr);
      break;
    case MLOG_UNDO_ERASE_END:
      ptr = PARSE_OR_APPLY_UNDO_ERASE_PAGE_END(ptr, end_ptr, nullptr);
      break;
    case MLOG_UNDO_INIT:
      ptr = PARSE_OR_APPLY_UNDO_PAGE_INIT(ptr, end_ptr, nullptr);
      break;
    case MLOG_UNDO_HDR_DISCARD:
      ptr = ParseOrApplyTrxUndoDiscardLatest(ptr, end_ptr, nullptr);
      break;
    case MLOG_UNDO_HDR_CREATE:
    case MLOG_UNDO_HDR_REUSE:
      ptr = ParseOrApplyTrxUndoPageHeader(type, ptr, end_ptr,nullptr);
      break;
    case MLOG_REC_MIN_MARK:
    case MLOG_COMP_REC_MIN_MARK:
      ptr = ParseOrApplySetMinRecMark(ptr,
                                      end_ptr,
                                      type == MLOG_COMP_REC_MIN_MARK,
                                      nullptr);
      break;
    case MLOG_REC_DELETE:
    case MLOG_COMP_REC_DELETE:
      if (nullptr != (ptr = mlog_parse_index(ptr, end_ptr,
                                             type == MLOG_COMP_REC_DELETE))) {

        ptr = ParseOrApplyDeleteRec(ptr, end_ptr,nullptr);
      }
      break;
    case MLOG_IBUF_BITMAP_INIT:
      /* Allow anything in page_type when creating a page. */
      ptr = ParseOrApplyIbufBitmapInit(ptr, end_ptr, nullptr);
      break;
    case MLOG_INIT_FILE_PAGE:
    case MLOG_INIT_FILE_PAGE2:
      // 该类型的日志解析不会干任何事，没有body
      break;
    case MLOG_WRITE_STRING:
      ptr = ParseOrApplyString(ptr, end_ptr, nullptr);
      break;
      // ZIP页不管它
    case MLOG_ZIP_WRITE_NODE_PTR:
    case MLOG_ZIP_WRITE_BLOB_PTR:
    case MLOG_ZIP_WRITE_HEADER:
    case MLOG_ZIP_PAGE_COMPRESS:
    case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:
      break;
    default:
      ptr = nullptr;
      std::cerr << "found unknown log type." << std::endl;
  }

  return(ptr);
}

uint32_t ParseSingleLogRecord(LOG_TYPE &type,
                     const byte* ptr,
                     const byte* end_ptr,
                     space_id_t &space_id,
                     page_id_t &page_id,
                     byte** body) {
  const byte*	new_ptr = ptr;
  *body = nullptr;
  if (new_ptr >= end_ptr) {
    return 0;
  }

  // 1. 特殊的log特殊处理
  type = static_cast<LOG_TYPE>(mach_read_from_1(new_ptr));
  // MLOG_MULTI_REC_END、LOG_TYPE::MLOG_DUMMY_RECORD、LOG_TYPE::MLOG_CHECKPOINT三种log具有固定的长度
  if (type == LOG_TYPE::MLOG_MULTI_REC_END || type ==  LOG_TYPE::MLOG_DUMMY_RECORD) {
    return 1;
  }
  if (type == LOG_TYPE::MLOG_CHECKPOINT) {
    if (end_ptr < new_ptr + SIZE_OF_MLOG_CHECKPOINT) {
      return 0;
    }
    return SIZE_OF_MLOG_CHECKPOINT;
  }

  // 2. 解析type
  if (end_ptr < new_ptr + 1) {
    *body = nullptr;
    return 0;
  }
  type = static_cast<LOG_TYPE>((static_cast<uint8_t>(*new_ptr) & ~MLOG_SINGLE_REC_FLAG));
  assert(type <= MLOG_BIGGEST_TYPE);
  new_ptr++;

  // 3. 解析space id和page id，space id和page id使用compressed格式存储，最多会占用5字节
  if (end_ptr < new_ptr + 2) {
    *body = nullptr;
    return 0;
  }

  space_id = mach_parse_compressed(&new_ptr, end_ptr);

  if (new_ptr != nullptr) {
    page_id = mach_parse_compressed(&new_ptr, end_ptr);
  }
  if (new_ptr == nullptr) {
    return 0;
  }
  *body = const_cast<byte *>(new_ptr);
  // 4. 解析log body
  new_ptr = ParseSingleLogRecordBody(type, const_cast<byte *>(new_ptr), end_ptr, space_id, page_id);

  if (new_ptr == nullptr) return 0;
  return(new_ptr - ptr);
}

}
