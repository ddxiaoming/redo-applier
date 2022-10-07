#include "parse.h"
#include "utility.h"
#include "config.h"
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
static byte* ParseOrApplyNBytes(LOG_TYPE type, const byte* ptr, const byte*	end_ptr, byte *page) {
  uint16_t offset;
  uint32_t val;
  assert(type <= MLOG_8BYTES);
  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  // 读出偏移量
  offset = mach_read_from_2(ptr);
  ptr += 2;

  if (offset >= DATA_PAGE_SIZE) {
    return nullptr;
  }

  if (type == MLOG_8BYTES) {
    auto dval = mach_u64_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }
    // 对MLOG_8BYTES类型的日志进行Apply
    if (page) {
      mach_write_to_8(page + offset, dval);
    }
    return const_cast<byte*>(ptr);
  }

  // 读出value
  val = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return nullptr;
  }

  switch (type) {
    case MLOG_1BYTE:
      // 日志损坏
      if (val > 0xFFUL) {
        ptr = nullptr;
      }
      // 对MLOG_1BYTES类型的日志进行Apply
      if (page) {
        mach_write_to_1(page + offset, val);
      }
      break;
    case MLOG_2BYTES:
      // 日志损坏
      if (val > 0xFFFFUL) {
        ptr = nullptr;
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

  return const_cast<byte*>(ptr);
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
row_upd_index_parse(const byte*	ptr, const byte*	end_ptr) {
  if (end_ptr < ptr + 1) {
    return nullptr;
  }
  ptr++;
  uint32_t n_fields = mach_parse_compressed(&ptr, end_ptr);
  if (ptr == nullptr) {
    return nullptr;
  }
  for (int i = 0; i < n_fields; i++) {
    mach_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }
    uint32_t len = mach_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }

    if (len != 0xFFFFFFFF) {
      if (end_ptr < ptr + len) {
        return nullptr;
      }
      ptr += len;
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

  ptr = row_upd_index_parse(ptr, end_ptr);

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

static inline uint32_t
rec_get_bit_field_1(const byte*	rec, uint32_t offs, uint32_t mask, uint32_t shift) {
  return((mach_read_from_1(rec - offs) & mask) >> shift);
}

static inline bool rec_info_bits_valid(uint32_t bits) {
  return(0 == (bits & ~(REC_INFO_DELETED_FLAG | REC_INFO_MIN_REC_FLAG)));
}

void rec_set_bit_field_1(byte*	rec, uint32_t	val, uint32_t	offs, uint32_t mask, uint32_t shift) {
  mach_write_to_1(rec - offs,
                  (mach_read_from_1(rec - offs) & ~mask)
                  | (val << shift));
}

static inline uint32_t rec_get_info_bits(const byte*	rec, bool comp) {
  const uint32_t	val = rec_get_bit_field_1(
      rec, comp ? REC_NEW_INFO_BITS : REC_OLD_INFO_BITS,
      REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
  assert(rec_info_bits_valid(val));
  return(val);
}
static inline void rec_set_info_bits_new(byte*	rec, uint32_t bits) {
  assert(rec_info_bits_valid(bits));
  rec_set_bit_field_1(rec, bits, REC_NEW_INFO_BITS,
                      REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
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

static uint32_t
page_dir_find_owner_slot(const byte *page, const byte *rec) {
  uint16_t rec_offs_bytes;
  const byte*	slot;
  const byte*	first_slot;
  const byte*	r = rec;

  first_slot = page_dir_get_nth_slot(page, 0);
  slot = page_dir_get_nth_slot(page, page_dir_get_n_slots(page) - 1);


  while (rec_get_n_owned_new(r) == 0) {
    // 获取下一条记录
    r = page + mach_read_from_2(rec - 2);
  }


  rec_offs_bytes = mach_encode_2(r - page);

  while (*(uint16_t *) slot != rec_offs_bytes) {
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
  return(page + mach_read_from_2(slot));
}

static const byte *rec_get_next_ptr_const(const byte *page, const byte* rec) {
  uint32_t field_value;
  field_value = mach_read_from_2(rec - REC_NEXT);
  if (field_value == 0) {
    return nullptr;
  }

  return page + field_value;
}
static inline uint32_t rec_get_next_offs(const byte *page, const byte *rec) {
  uint32_t field_value;
  field_value = mach_read_from_2(rec - REC_NEXT);
  if (field_value == 0) {
    return(0);
  }
  return rec + field_value - page;
}
const byte* page_rec_get_next(const byte* page, const byte*	rec) {
  uint32_t offs;
  offs = rec_get_next_offs(page, rec);
  assert(offs <= DATA_PAGE_SIZE);
  if (offs == 0) {
    return nullptr;
  }
  return page + offs;
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
static void
ApplyCompPageCreate(byte* ptr, const byte* end_ptr, byte* page) {
  if (page == nullptr) {
    return;
  }

  // 设置page的种类
  fil_page_set_type(page, FIL_PAGE_INDEX);

  std::memset(page + PAGE_HEADER, 0, PAGE_HEADER_PRIV_END);
  page[PAGE_HEADER + PAGE_N_DIR_SLOTS + 1] = 2; // 初始化PAGE_N_DIR_SLOTS属性
  page[PAGE_HEADER + PAGE_DIRECTION + 1] = PAGE_NO_DIRECTION; // // 初始化PAGE_DIRECTION属性

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
static void
ApplyInitFilePage2(byte* ptr, const byte* end_ptr, byte* page) {
  if (page == nullptr) {
    return;
  }

  // 设置page的种类
  fil_page_set_type(page, FIL_PAGE_INDEX);

  std::memset(page + PAGE_HEADER, 0, PAGE_HEADER_PRIV_END);
  page[PAGE_HEADER + PAGE_N_DIR_SLOTS + 1] = 2; // 初始化PAGE_N_DIR_SLOTS属性
  page[PAGE_HEADER + PAGE_DIRECTION + 1] = PAGE_NO_DIRECTION; // // 初始化PAGE_DIRECTION属性

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
static byte*
ParseOrApplyString(byte* ptr, const byte* end_ptr, byte* page) {
  uint32_t offset;
  uint32_t len;

  if (end_ptr < ptr + 4) {

    return nullptr;
  }

  offset = mach_read_from_2(ptr);
  ptr += 2;
  len = mach_read_from_2(ptr);
  ptr += 2;

  assert(offset < DATA_PAGE_SIZE);
  assert(len + offset <= DATA_PAGE_SIZE);

  if (end_ptr < ptr + len) {
    return nullptr;
  }

  // Apply
  if (page) {
    memcpy(page + offset, ptr, len);
  }

  return(ptr + len);
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
  *body = const_cast<byte *>(ptr);
  // 4. 解析log body
  new_ptr = ParseSingleLogRecordBody(type, const_cast<byte *>(new_ptr), end_ptr, space_id, page_id);

  if (new_ptr == nullptr) return 0;
  return(new_ptr - ptr);
}

}
