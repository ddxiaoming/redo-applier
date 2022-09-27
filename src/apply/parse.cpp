#include "parse.h"
#include "utility.h"
#include "config.h"
#include <cassert>
namespace Lemon {

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
static byte* PARSE_MLOG_NBYTES(LOG_TYPE	type, const byte*	ptr, const byte*	end_ptr) {
  uint16_t offset;
  uint32_t val;
  assert(type <= MLOG_8BYTES);
  if (end_ptr < ptr + 2) {
    return nullptr;
  }

  offset = mach_read_from_2(ptr);
  ptr += 2;

  if (offset >= DATA_PAGE_SIZE) {
    return nullptr;
  }

  if (type == MLOG_8BYTES) {
    mach_u64_parse_compressed(&ptr, end_ptr);
    if (ptr == nullptr) {
      return nullptr;
    }
    return const_cast<byte*>(ptr);
  }

  val = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return nullptr;
  }

  switch (type) {
    case MLOG_1BYTE:
      if (val > 0xFFUL) {
        ptr = nullptr;
      }
      break;
    case MLOG_2BYTES:
      if (val > 0xFFFFUL) {
        ptr = nullptr;
      }
      break;
    default:
      break;
  }

  return(const_cast<byte*>(ptr));
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
static byte*
PARSE_MLOG_REC_SEC_DELETE_MARK(byte* ptr, const byte* end_ptr) {
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
PARSE_MLOG_REC_UPDATE_IN_PLACE(byte* ptr, byte* end_ptr) {
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
    byte*		end_ptr/*!< in: buffer end */) {
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
PARSE_COPY_REC_LIST_TO_CREATED_PAGE(byte* ptr, byte* end_ptr) {
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

/***********************************************************//**
Parses a redo log record of reorganizing a page.
@return end of log record or NULL */
byte*
PARSE_PAGE_REORGANIZE(
/*======================*/
    byte*		ptr,	/*!< in: buffer */
    byte*		end_ptr,/*!< in: buffer end */
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
                                      byte* end_ptr,
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
      ptr = PARSE_MLOG_NBYTES(type, ptr, end_ptr);
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
    case MLOG_PAGE_CREATE:
    case MLOG_COMP_PAGE_CREATE:
      page_parse_create(block, type == MLOG_COMP_PAGE_CREATE, false);
      break;
    case MLOG_PAGE_CREATE_RTREE: case MLOG_COMP_PAGE_CREATE_RTREE:
      page_parse_create(block, type == MLOG_COMP_PAGE_CREATE_RTREE,
                        true);
      break;
    case MLOG_UNDO_INSERT:
      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
      ptr = trx_undo_parse_add_undo_rec(ptr, end_ptr, page);
      break;
    case MLOG_UNDO_ERASE_END:
      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
      ptr = trx_undo_parse_erase_page_end(ptr, end_ptr, page, mtr);
      break;
    case MLOG_UNDO_INIT:
      /* Allow anything in page_type when creating a page. */
      ptr = trx_undo_parse_page_init(ptr, end_ptr, page, mtr);
      break;
    case MLOG_UNDO_HDR_DISCARD:
      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
      ptr = trx_undo_parse_discard_latest(ptr, end_ptr, page, mtr);
      break;
    case MLOG_UNDO_HDR_CREATE:
    case MLOG_UNDO_HDR_REUSE:
      ut_ad(!page || page_type == FIL_PAGE_UNDO_LOG);
      ptr = trx_undo_parse_page_header(type, ptr, end_ptr,
                                       page, mtr);
      break;
    case MLOG_REC_MIN_MARK: case MLOG_COMP_REC_MIN_MARK:
      ut_ad(!page || fil_page_type_is_index(page_type));
      /* On a compressed page, MLOG_COMP_REC_MIN_MARK
      will be followed by MLOG_COMP_REC_DELETE
      or MLOG_ZIP_WRITE_HEADER(FIL_PAGE_PREV, FIL_nullptr)
      in the same mini-transaction. */
      ut_a(type == MLOG_COMP_REC_MIN_MARK || !page_zip);
      ptr = btr_parse_set_min_rec_mark(
          ptr, end_ptr, type == MLOG_COMP_REC_MIN_MARK,
          page, mtr);
      break;
    case MLOG_REC_DELETE: case MLOG_COMP_REC_DELETE:
      ut_ad(!page || fil_page_type_is_index(page_type));

      if (nullptr != (ptr = mlog_parse_index(
          ptr, end_ptr,
          type == MLOG_COMP_REC_DELETE,
          &index))) {
        ut_a(!page
             || (ibool)!!page_is_comp(page)
                       == dict_table_is_comp(index->table));
        ptr = page_cur_parse_delete_rec(ptr, end_ptr,
                                        block, index, mtr);
      }
      break;
    case MLOG_IBUF_BITMAP_INIT:
      /* Allow anything in page_type when creating a page. */
      ptr = ibuf_parse_bitmap_init(ptr, end_ptr, block, mtr);
      break;
    case MLOG_INIT_FILE_PAGE:
    case MLOG_INIT_FILE_PAGE2:
      /* Allow anything in page_type when creating a page. */
      ptr = fsp_parse_init_file_page(ptr, end_ptr, block);
      break;
    case MLOG_WRITE_STRING:
      ut_ad(!page || page_type != FIL_PAGE_TYPE_ALLOCATED
            || page_no == 0);
      ptr = mlog_parse_string(ptr, end_ptr, page, page_zip);
      break;
    case MLOG_ZIP_WRITE_NODE_PTR:
      ut_ad(!page || fil_page_type_is_index(page_type));
      ptr = page_zip_parse_write_node_ptr(ptr, end_ptr,
                                          page, page_zip);
      break;
    case MLOG_ZIP_WRITE_BLOB_PTR:
      ut_ad(!page || fil_page_type_is_index(page_type));
      ptr = page_zip_parse_write_blob_ptr(ptr, end_ptr,
                                          page, page_zip);
      break;
    case MLOG_ZIP_WRITE_HEADER:
      ut_ad(!page || fil_page_type_is_index(page_type));
      ptr = page_zip_parse_write_header(ptr, end_ptr,
                                        page, page_zip);
      break;
    case MLOG_ZIP_PAGE_COMPRESS:
      /* Allow anything in page_type when creating a page. */
      ptr = page_zip_parse_compress(ptr, end_ptr,
                                    page, page_zip);
      break;
    case MLOG_ZIP_PAGE_COMPRESS_NO_DATA:
      if (nullptr != (ptr = mlog_parse_index(
          ptr, end_ptr, TRUE, &index))) {

        ut_a(!page || ((ibool)!!page_is_comp(page)
                              == dict_table_is_comp(index->table)));
        ptr = page_zip_parse_compress_no_data(
            ptr, end_ptr, page, page_zip, index);
      }
      break;
    default:
      ptr = nullptr;
      recv_sys->found_corrupt_log = true;
  }

  return(ptr);
}

uint32_t
ParseSingleLogRecord(LOG_TYPE &type,
                     const byte* ptr,
                     const byte* end_ptr,
                     space_id_t &space_id,
                     page_id_t &page_id,
                     byte** body) {
  byte*	new_ptr;
  *body = nullptr;
  if (ptr >= end_ptr) {
    return 0;
  }

  // 1. 特殊的log特殊处理
  type = static_cast<LOG_TYPE>(mach_read_from_1(ptr));
  // MLOG_MULTI_REC_END、LOG_TYPE::MLOG_DUMMY_RECORD、LOG_TYPE::MLOG_CHECKPOINT三种log具有固定的长度
  if (type == LOG_TYPE::MLOG_MULTI_REC_END || type ==  LOG_TYPE::MLOG_DUMMY_RECORD) {
    return 1;
  }
  if (type == LOG_TYPE::MLOG_CHECKPOINT) {
    if (end_ptr < ptr + SIZE_OF_MLOG_CHECKPOINT) {
      return 0;
    }
    return SIZE_OF_MLOG_CHECKPOINT;
  }

  // 2. 解析type
  if (end_ptr < ptr + 1) {
    *body = nullptr;
    return 0;
  }
  type = static_cast<LOG_TYPE>((static_cast<uint8_t>(*ptr) & ~MLOG_SINGLE_REC_FLAG));
  assert(type <= MLOG_BIGGEST_TYPE);
  ptr++;

  // 3. 解析space id和page id，space id和page id使用compressed格式存储，最多会占用5字节
  if (end_ptr < ptr + 2) {
    *body = nullptr;
    return 0;
  }

  space_id = mach_parse_compressed(&ptr, end_ptr);

  if (ptr != nullptr) {
    page_id = mach_parse_compressed(&ptr, end_ptr);
  }

  // 4. 解析log body
  new_ptr = recv_parse_or_apply_log_rec_body(
      *type, new_ptr, end_ptr, *space, *page_no, nullptr, nullptr);

  if (UNIV_UNLIKELY(new_ptr == nullptr)) {

    return(0);
  }

  return(new_ptr - ptr);
}

}
