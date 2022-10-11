#pragma once
#include <cinttypes>
namespace Lemon {

using page_id_t = uint32_t;
using space_id_t = uint32_t;
using frame_id_t = uint32_t;
using lsn_t = uint64_t;
using size_t = uint64_t;
using byte = unsigned char;
using trx_id_t = uint64_t;
using roll_ptr_t = uint64_t;
// log block size in bytes
static constexpr size_t LOG_BLOCK_SIZE = 512;

// data page size in bytes
static constexpr size_t DATA_PAGE_SIZE = 16 * 1024; // 16KB

static constexpr space_id_t REDO_LOG_SPACE_ID = 0xFFFFFFF0UL;

// 第一个LOG_BLOCK_HDR_NO从18开始
static constexpr uint32_t FIRST_LOG_BLOCK_HDR_NO = 18;

// 有4个 redo log block 放的是metadata
static constexpr uint32_t N_LOG_METADATA_BLOCKS = 4;

static constexpr lsn_t LOG_START_LSN = 8716;

static constexpr uint32_t PER_LOG_FILE_SIZE = 48 * 1024 * 1204; // 48M

static constexpr uint32_t N_BLOCKS_IN_A_PAGE = DATA_PAGE_SIZE / LOG_BLOCK_SIZE; // 48M

static constexpr uint32_t BUFFER_POOL_SIZE = 8 * 1024 * 1024; // buffer pool size in data_page_size

enum LOG_TYPE : uint8_t {
  /** if the mtr contains only one log record for one page,
  i.e., write_initial_log_record has been called only once,
  this flag is ORed to the type of that first log record */
  MLOG_SINGLE_REC_FLAG = 128,

  /** one byte is written */
  MLOG_1BYTE = 1,

  /** 2 bytes ... */
  MLOG_2BYTES = 2,

  /** 4 bytes ... */
  MLOG_4BYTES = 4,

  /** 8 bytes ... */
  MLOG_8BYTES = 8,

  /** Record insert */
  MLOG_REC_INSERT = 9,

  /** Mark clustered index record deleted */
  MLOG_REC_CLUST_DELETE_MARK = 10,

  /** Mark secondary index record deleted */
  MLOG_REC_SEC_DELETE_MARK = 11,

  /** update of a record, preserves record field sizes */
  MLOG_REC_UPDATE_IN_PLACE = 13,

  /*!< Delete a record from a page */
  MLOG_REC_DELETE = 14,

  /** Delete record list end on index page */
  MLOG_LIST_END_DELETE = 15,

  /** Delete record list start on index page */
  MLOG_LIST_START_DELETE = 16,

  /** Copy record list end to a new created index page */
  MLOG_LIST_END_COPY_CREATED = 17,

  /** Reorganize an index page in ROW_FORMAT=REDUNDANT */
  MLOG_PAGE_REORGANIZE = 18,

  /** Create an index page */
  MLOG_PAGE_CREATE = 19,

  /** Insert entry in an undo log */
  MLOG_UNDO_INSERT = 20,

  /** erase an undo log page end */
  MLOG_UNDO_ERASE_END = 21,

  /** initialize a page in an undo log */
  MLOG_UNDO_INIT = 22,

  /** discard an update undo log header */
  MLOG_UNDO_HDR_DISCARD = 23,

  /** reuse an insert undo log header */
  MLOG_UNDO_HDR_REUSE = 24,

  /** create an undo log header */
  MLOG_UNDO_HDR_CREATE = 25,

  /** mark an index record as the predefined minimum record */
  MLOG_REC_MIN_MARK = 26,

  /** initialize an ibuf bitmap page */
  MLOG_IBUF_BITMAP_INIT = 27,

  /** this means that a file page is taken into use and the prior
  contents of the page should be ignored: in recovery we must not
  trust the lsn values stored to the file page.
  Note: it's deprecated because it causes crash recovery problem
  in bulk create index, and actually we don't need to reset page
  lsn in recv_recover_page_func() now. */
  MLOG_INIT_FILE_PAGE = 29,

  /** write a string to a page */
  MLOG_WRITE_STRING = 30,

  /** If a single mtr writes several log records, this log
  record ends the sequence of these records */
  MLOG_MULTI_REC_END = 31,

  /** dummy log record used to pad a log block full */
  MLOG_DUMMY_RECORD = 32,

  /** log record about an .ibd file creation */
  //MLOG_FILE_CREATE = 33,

  /** rename databasename/tablename (no .ibd file name suffix) */
  //MLOG_FILE_RENAME = 34,

  /** delete a tablespace file that starts with (space_id,page_no) */
  MLOG_FILE_DELETE = 35,

  /** mark a compact index record as the predefined minimum record */
  MLOG_COMP_REC_MIN_MARK = 36,

  /** create a compact index page */
  MLOG_COMP_PAGE_CREATE = 37,

  /** compact record insert */
  MLOG_COMP_REC_INSERT = 38,

  /** mark compact clustered index record deleted */
  MLOG_COMP_REC_CLUST_DELETE_MARK = 39,

  /** mark compact secondary index record deleted; this log
  record type is redundant, as MLOG_REC_SEC_DELETE_MARK is
  independent of the record format. */
  MLOG_COMP_REC_SEC_DELETE_MARK = 40,

  /** update of a compact record, preserves record field sizes */
  MLOG_COMP_REC_UPDATE_IN_PLACE = 41,

  /** delete a compact record from a page */
  MLOG_COMP_REC_DELETE = 42,

  /** delete compact record list end on index page */
  MLOG_COMP_LIST_END_DELETE = 43,

  /*** delete compact record list start on index page */
  MLOG_COMP_LIST_START_DELETE = 44,

  /** copy compact record list end to a new created index page */
  MLOG_COMP_LIST_END_COPY_CREATED = 45,

  /** reorganize an index page */
  MLOG_COMP_PAGE_REORGANIZE = 46,

  /** log record about creating an .ibd file, with format */
  MLOG_FILE_CREATE2 = 47,

  /** write the node pointer of a record on a compressed
  non-leaf B-tree page */
  MLOG_ZIP_WRITE_NODE_PTR = 48,

  /** write the BLOB pointer of an externally stored column
  on a compressed page */
  MLOG_ZIP_WRITE_BLOB_PTR = 49,

  /** write to compressed page header */
  MLOG_ZIP_WRITE_HEADER = 50,

  /** compress an index page */
  MLOG_ZIP_PAGE_COMPRESS = 51,

  /** compress an index page without logging it's image */
  MLOG_ZIP_PAGE_COMPRESS_NO_DATA = 52,

  /** reorganize a compressed page */
  MLOG_ZIP_PAGE_REORGANIZE = 53,

  /** rename a tablespace file that starts with (space_id,page_no) */
  MLOG_FILE_RENAME2 = 54,

  /** note the first use of a tablespace file since checkpoint */
  MLOG_FILE_NAME = 55,

  /** note that all buffered log was written since a checkpoint */
  MLOG_CHECKPOINT = 56,

  /** Create a R-Tree index page */
  MLOG_PAGE_CREATE_RTREE = 57,

  /** create a R-tree compact page */
  MLOG_COMP_PAGE_CREATE_RTREE = 58,

  /** this means that a file page is taken into use.
  We use it to replace MLOG_INIT_FILE_PAGE. */
  MLOG_INIT_FILE_PAGE2 = 59,

  /** Table is being truncated. (Marked only for file-per-table) */
  MLOG_TRUNCATE = 60,

  /** notify that an index tree is being loaded without writing
  redo log about individual pages */
  MLOG_INDEX_LOAD = 61,

  /** biggest value (used in assertions) */
  MLOG_BIGGEST_TYPE = MLOG_INDEX_LOAD
};


// redo log 相关的偏移量
static constexpr uint32_t LOG_BLOCK_HDR_NO = 0;
static constexpr uint32_t LOG_BLOCK_FLUSH_BIT_MASK = 0x80000000UL;
static constexpr uint32_t LOG_BLOCK_HDR_DATA_LEN = 4;
static constexpr uint32_t LOG_BLOCK_FIRST_REC_GROUP = 6;
static constexpr uint32_t LOG_BLOCK_CHECKPOINT_NO	= 8;
static constexpr uint32_t LOG_BLOCK_HDR_SIZE = 12;
static constexpr uint32_t LOG_BLOCK_CHECKSUM = 4;
static constexpr uint32_t	LOG_BLOCK_TRL_SIZE = 4;
static constexpr uint32_t LOG_CHECKPOINT_NO = 0;
static constexpr uint32_t LOG_CHECKPOINT_LSN = 8;
static constexpr uint32_t LOG_CHECKPOINT_OFFSET	= 16;
static constexpr uint32_t LOG_HEADER_FORMAT = 0;
static constexpr uint32_t LOG_HEADER_PAD1	= 4;
static constexpr uint32_t LOG_HEADER_START_LSN = 8;
static constexpr uint32_t LOG_HEADER_CREATOR = 16;
static constexpr uint32_t LOG_HEADER_CREATOR_END = (LOG_HEADER_CREATOR + 32);
static constexpr uint32_t LOG_HEADER_FORMAT_CURRENT = 1;
static constexpr uint32_t LOG_CHECKPOINT_1 = LOG_BLOCK_SIZE;
static constexpr uint32_t LOG_CHECKPOINT_2 = (3 * LOG_BLOCK_SIZE);
static constexpr uint32_t LOG_FILE_HDR_SIZE	(4 * LOG_BLOCK_SIZE);

static constexpr unsigned char infimum_supremum_compact[] = {
    /* the infimum record */
    0x01/*n_owned=1*/,
    0x00, 0x02/* heap_no=0, REC_STATUS_INFIMUM */,
    0x00, 0x0d/* pointer to supremum */,
    'i', 'n', 'f', 'i', 'm', 'u', 'm', 0,
    /* the supremum record */
    0x01/*n_owned=1*/,
    0x00, 0x0b/* heap_no=1, REC_STATUS_SUPREMUM */,
    0x00, 0x00/* end of record list */,
    's', 'u', 'p', 'r', 'e', 'm', 'u', 'm'
};

static constexpr uint32_t SIZE_OF_MLOG_CHECKPOINT = 9;



// undo log相关的常量
static constexpr uint32_t TRX_UNDO_PAGE_HDR = 38U;
static constexpr uint32_t	TRX_UNDO_PAGE_TYPE = 0;
static constexpr uint32_t	TRX_UNDO_PAGE_START = 2;
static constexpr uint32_t TRX_UNDO_PAGE_FREE = 4;
static constexpr uint32_t TRX_UNDO_PAGE_HDR_SIZE = 6 + 2 * 6;
static constexpr uint32_t FIL_PAGE_DATA_END = 8;
static constexpr uint32_t	TRX_UNDO_SEG_HDR = (TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);
static constexpr uint32_t	TRX_UNDO_LAST_LOG = 2;
static constexpr uint32_t TRX_UNDO_NEXT_LOG = 30;
static constexpr uint32_t TRX_UNDO_PREV_LOG = 32;
static constexpr uint32_t	TRX_UNDO_INSERT = 1;
static constexpr uint32_t TRX_UNDO_UPDATE = 2;
static constexpr uint32_t TRX_UNDO_STATE = 0;
static constexpr uint32_t TRX_UNDO_ACTIVE = 1;
static constexpr uint32_t TRX_UNDO_CACHED = 2;
static constexpr uint32_t	TRX_UNDO_TO_FREE = 3;
static constexpr uint32_t TRX_UNDO_TO_PURGE = 4;
static constexpr uint32_t	TRX_UNDO_PREPARED = 5;

static constexpr uint32_t TRX_UNDO_LOG_OLD_HDR_SIZE = 34 + 2 * 6;



static constexpr uint32_t REC_OLD_INFO_BITS = 6;
static constexpr uint32_t REC_NEW_INFO_BITS = 5;
static constexpr uint32_t REC_INFO_BITS_MASK = 0xF0UL;
static constexpr uint32_t REC_INFO_BITS_SHIFT = 0;
static constexpr uint32_t REC_INFO_MIN_REC_FLAG = 0x10UL;
static constexpr uint32_t REC_INFO_DELETED_FLAG = 0x20UL;

// page相关的常量
static constexpr uint32_t PAGE_DIR = FIL_PAGE_DATA_END;
static constexpr uint32_t	PAGE_DIR_SLOT_SIZE = 2;
static constexpr uint32_t PAGE_N_DIR_SLOTS = 0;
static constexpr uint32_t	PAGE_LAST_INSERT = 10;
static constexpr uint32_t	PAGE_INDEX_ID = 28;
static constexpr uint32_t FIL_PAGE_DATA = 38U;
static constexpr uint32_t FSEG_PAGE_DATA = FIL_PAGE_DATA;
static constexpr uint32_t	PAGE_HEADER = FSEG_PAGE_DATA;
static constexpr uint32_t REC_OLD_N_OWNED	= 6;	/* This is single byte bit-field */
static constexpr uint32_t REC_NEW_N_OWNED = 5;	/* This is single byte bit-field */
static constexpr uint32_t REC_N_OWNED_MASK = 0xFUL;
static constexpr uint32_t REC_N_OWNED_SHIFT = 0;
static constexpr uint32_t REC_NEXT = 2;
static constexpr uint32_t PAGE_HEADER_PRIV_END = 26;
static constexpr uint32_t PAGE_DIRECTION = 12;
static constexpr uint32_t PAGE_NO_DIRECTION = 5;
static constexpr uint32_t PAGE_N_HEAP = 4;
static constexpr uint32_t PAGE_HEAP_TOP = 2;
static constexpr uint32_t PAGE_HEAP_NO_USER_LOW = 2;
static constexpr uint32_t FSEG_HEADER_SIZE = 10;
static constexpr uint32_t REC_N_NEW_EXTRA_BYTES = 5;
static constexpr uint32_t PAGE_DATA = (PAGE_HEADER + 36 + 2 * FSEG_HEADER_SIZE);
static constexpr uint32_t PAGE_NEW_SUPREMUM = (PAGE_DATA + 2 * REC_N_NEW_EXTRA_BYTES + 8);
static constexpr uint32_t PAGE_NEW_INFIMUM = (PAGE_DATA + REC_N_NEW_EXTRA_BYTES);
static constexpr uint32_t PAGE_NEW_SUPREMUM_END = (PAGE_NEW_SUPREMUM + 8);
static constexpr uint32_t FIL_PAGE_LSN = 16;
static constexpr uint32_t FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID = 34;
static constexpr uint32_t FIL_PAGE_OFFSET = 4;
static constexpr uint32_t FIL_PAGE_END_LSN_OLD_CHKSUM = 8;
// trx相关的常量
static constexpr uint32_t XIDDATASIZE = 128;
static constexpr uint32_t	TRX_UNDO_XA_FORMAT = (TRX_UNDO_LOG_OLD_HDR_SIZE);
static constexpr uint32_t	TRX_UNDO_XA_TRID_LEN = (TRX_UNDO_XA_FORMAT + 4);
static constexpr uint32_t	TRX_UNDO_XA_BQUAL_LEN = (TRX_UNDO_XA_TRID_LEN + 4);
static constexpr uint32_t	TRX_UNDO_XA_XID = (TRX_UNDO_XA_BQUAL_LEN + 4);
static constexpr uint32_t TRX_UNDO_LOG_XA_HDR_SIZE = (TRX_UNDO_XA_XID + XIDDATASIZE);
static constexpr uint32_t TRX_UNDO_TRX_ID = 0;
static constexpr uint32_t	TRX_UNDO_TRX_NO = 8;
static constexpr uint32_t TRX_UNDO_DEL_MARKS = 16;
static constexpr uint32_t TRX_UNDO_LOG_START = 18;
static constexpr uint32_t	TRX_UNDO_XID_EXISTS = 20;
static constexpr uint32_t TRX_UNDO_DICT_TRANS = 21;

static constexpr uint32_t	FIL_ADDR_SIZE = 6;
static constexpr uint32_t FLST_BASE_NODE_SIZE	(4 + 2 * FIL_ADDR_SIZE);
static constexpr uint32_t TRX_UNDO_SEG_HDR_SIZE = (4 + FSEG_HEADER_SIZE + FLST_BASE_NODE_SIZE);
// page的类型
/** File page types (values of FIL_PAGE_TYPE) @{ */
static constexpr uint32_t FIL_PAGE_INDEX = 17855;	/*!< B-tree node */
static constexpr uint32_t FIL_PAGE_RTREE= 17854;	/*!< B-tree node */
static constexpr uint32_t FIL_PAGE_UNDO_LOG = 2;	/*!< Undo log page */
static constexpr uint32_t FIL_PAGE_INODE = 3;	/*!< Index node */
static constexpr uint32_t FIL_PAGE_IBUF_FREE_LIST = 4;	/*!< Insert buffer free list */
/* File page types introduced in MySQL/InnoDB 5.1.7 */
static constexpr uint32_t FIL_PAGE_TYPE_ALLOCATED = 0;	/*!< Freshly allocated page */
static constexpr uint32_t FIL_PAGE_IBUF_BITMAP = 5;	/*!< Insert buffer bitmap */
static constexpr uint32_t FIL_PAGE_TYPE_SYS = 6;	/*!< System page */
static constexpr uint32_t FIL_PAGE_TYPE_TRX_SYS = 7;	/*!< Transaction system data */
static constexpr uint32_t FIL_PAGE_TYPE_FSP_HDR = 8;	/*!< File space header */
static constexpr uint32_t FIL_PAGE_TYPE_XDES = 9;	/*!< Extent descriptor page */
static constexpr uint32_t FIL_PAGE_TYPE_BLOB = 10;	/*!< Uncompressed BLOB page */
static constexpr uint32_t FIL_PAGE_TYPE_ZBLOB = 11;	/*!< First compressed BLOB page */
static constexpr uint32_t FIL_PAGE_TYPE_ZBLOB2 = 12;	/*!< Subsequent compressed BLOB page */
static constexpr uint32_t FIL_PAGE_TYPE_UNKNOWN = 13;	/*!< In old tablespaces, garbage
					in FIL_PAGE_TYPE is replaced with this
					value when flushing pages. */
static constexpr uint32_t FIL_PAGE_COMPRESSED = 14;	/*!< Compressed page */
static constexpr uint32_t FIL_PAGE_ENCRYPTED = 15;	/*!< Encrypted page */
static constexpr uint32_t FIL_PAGE_COMPRESSED_AND_ENCRYPTED = 16;
/*!< Compressed and Encrypted page */
static constexpr uint32_t FIL_PAGE_ENCRYPTED_RTREE = 17;	/*!< Encrypted R-tree page */

/** Used by i_s.cc to index into the text description. */
static constexpr uint32_t FIL_PAGE_TYPE_LAST = FIL_PAGE_TYPE_UNKNOWN;

static constexpr uint32_t FIL_PAGE_TYPE = 24;


// index type
static constexpr uint32_t DICT_CLUSTERED = 1;	/*!< clustered index; for other than auto-generated clustered indexes,
				                                        also DICT_UNIQUE will be set */
static constexpr uint32_t DICT_UNIQUE = 2;	/*!< unique index */
static constexpr uint32_t	DICT_IBUF = 8;	/*!< insert buffer tree */
static constexpr uint32_t DICT_CORRUPT = 16;	/*!< bit to store the corrupted flag in SYS_INDEXES.TYPE */
static constexpr uint32_t DICT_FTS = 32;	/* FTS index; can't be combined with the other flags */
static constexpr uint32_t DICT_SPATIAL = 64;	/* SPATIAL index; can't be combined with the other flags */
static constexpr uint32_t DICT_VIRTUAL = 128;	/* Index on Virtual column */


// data type
/* Precise data types for system columns and the length of those columns;
NOTE: the values must run from 0 up in the order given! All codes must
be less than 256 */
static constexpr uint32_t DATA_ROW_ID = 0;	/* row id: a 48-bit integer */
static constexpr uint32_t DATA_ROW_ID_LEN = 6;	/* stored length for row id */

static constexpr uint32_t DATA_TRX_ID = 1;	/* transaction id: 6 bytes */
static constexpr uint32_t DATA_TRX_ID_LEN = 6;

static constexpr uint32_t DATA_ROLL_PTR = 2;	/* rollback data pointer: 7 bytes */
static constexpr uint32_t DATA_ROLL_PTR_LEN = 7;

static constexpr uint32_t DATA_N_SYS_COLS = 3;	/* number of system columns defined above */

static constexpr uint32_t DATA_ITT_N_SYS_COLS = 2;
/* number of system columns for intrinsic
temporary table */
}
