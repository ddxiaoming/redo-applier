#pragma once
#include <cstdint>
namespace Lemon {

using page_id_t = uint32_t;
using space_id_t = uint32_t;
using frame_id_t = uint32_t;
using lsn_t = uint64_t;
using size_t = uint64_t;
using byte = unsigned char;
using trx_id_t = uint64_t;
using roll_ptr_t = uint64_t;

static constexpr int DATA_PAGE_PARTITION0 = 0;
static constexpr int DATA_PAGE_PARTITION1 = 1;
static constexpr int DATA_PAGE_PARTITION2 = 2;
static constexpr int DATA_PAGE_PARTITION3 = 3;
static constexpr int DATA_PAGE_PARTITION4 = 4;
static constexpr int DATA_PAGE_PARTITION5 = 5;
static constexpr int DATA_PAGE_PARTITION6 = 6;
static constexpr int DATA_PAGE_PARTITION7 = 7;
static constexpr int DATA_PAGE_PARTITION8 = 8;
static constexpr int DATA_PAGE_PARTITION9 = 9;
static constexpr int DATA_PAGE_PARTITION10 = 10;
static constexpr int DATA_PAGE_PARTITION11 = 11;
static constexpr int DATA_PAGE_PARTITION12 = 12;
static constexpr int DATA_PAGE_PARTITION13 = 13;
static constexpr int DATA_PAGE_PARTITION14 = 14;
static constexpr int DATA_PAGE_PARTITION15 = 15;
static constexpr int DATA_PAGE_PARTITION16 = 16;
static constexpr int DATA_PAGE_PARTITION17 = 17;
static constexpr int DATA_PAGE_PARTITION18 = 18;
static constexpr int DATA_PAGE_PARTITION19 = 19;
static constexpr int LOG_PARTITION = 20;
static constexpr size_t PARTITION_SIZE = 256 << 20; // 256 MB
// log block size in bytes
static constexpr size_t LOG_BLOCK_SIZE = 512;

// data page size in bytes
static constexpr size_t DATA_PAGE_SIZE = 16 * 1024; // 16KB

// data page size in bytes
static constexpr size_t FLASH_PAGE_SIZE = 16 * 1024; // 16KB

static constexpr space_id_t REDO_LOG_SPACE_ID = 0xFFFFFFF0UL;

// 第一个LOG_BLOCK_HDR_NO从18开始
static constexpr uint32_t FIRST_LOG_BLOCK_HDR_NO = 18;

// 有4个 redo log block 放的是metadata
static constexpr uint32_t N_LOG_METADATA_BLOCKS = 4;

static constexpr lsn_t LOG_START_LSN = 8716;

static constexpr uint32_t PER_LOG_FILE_SIZE = 48 * 1024 * 1204; // 48M

static constexpr uint32_t N_BLOCKS_IN_A_PAGE = DATA_PAGE_SIZE / LOG_BLOCK_SIZE; // 48M

static constexpr uint32_t BUFFER_POOL_SIZE = 64 * 1024 * 1024; // buffer pool size in bytes

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
static constexpr uint32_t FIL_PAGE_DATA = 38U;
static constexpr uint32_t FSEG_PAGE_DATA = FIL_PAGE_DATA;
static constexpr uint32_t	PAGE_HEADER = FSEG_PAGE_DATA;
static constexpr uint32_t REC_OLD_N_OWNED	= 6;	/* This is single byte bit-field */
static constexpr uint32_t REC_NEW_N_OWNED = 5;	/* This is single byte bit-field */
static constexpr uint32_t REC_N_OWNED_MASK = 0xFUL;
static constexpr uint32_t REC_N_OWNED_SHIFT = 0;
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
static constexpr uint32_t IBUF_BITS_PER_PAGE = 4;
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

static constexpr uint32_t DATA_ITT_N_SYS_COLS = 2; /* number of system columns for intrinsic temporary table */
static constexpr uint32_t DATA_FTS_DOC_ID = 3;	/* Used as FTS DOC ID column */

static constexpr uint32_t DATA_SYS_PRTYPE_MASK = 0xF; /* mask to extract the above from prtype */

/* Flags ORed to the precise data type */
static constexpr uint32_t DATA_NOT_NULL = 256;	/* this is ORed to the precise type when
				the column is declared as NOT NULL */
static constexpr uint32_t DATA_UNSIGNED = 512;	/* this id ORed to the precise type when
				we have an unsigned integer type */
static constexpr uint32_t DATA_BINARY_TYPE = 1024;	/* if the data type is a binary character
				string, this is ORed to the precise type:
				this only holds for tables created with
				>= MySQL-4.0.14 */
/* #define	DATA_NONLATIN1	2048 This is a relic from < 4.1.2 and < 5.0.1.
				In earlier versions this was set for some
				BLOB columns.
*/
static constexpr uint32_t DATA_GIS_MBR = 2048;	/* Used as GIS MBR column */
static constexpr uint32_t DATA_MBR_LEN = 2 * 2 * sizeof(double); /* GIS MBR length*/

static constexpr uint32_t DATA_LONG_TRUE_VARCHAR = 4096;	/* this is ORed to the precise data
				type when the column is true VARCHAR where
				MySQL uses 2 bytes to store the data len;
				for shorter VARCHARs MySQL uses only 1 byte */
static constexpr uint32_t DATA_VIRTUAL = 8192;	/* Virtual column */


/*-------------------------------------------*/
/* The 'MAIN TYPE' of a column */
static constexpr uint32_t DATA_MISSING = 0;	/* missing column */
static constexpr uint32_t DATA_VARCHAR = 1;	/* character varying of the
				latin1_swedish_ci charset-collation; note
				that the MySQL format for this, DATA_BINARY,
				DATA_VARMYSQL, is also affected by whether the
				'precise type' contains
				DATA_MYSQL_TRUE_VARCHAR */
static constexpr uint32_t DATA_CHAR	= 2;	/* fixed length character of the
				latin1_swedish_ci charset-collation */
static constexpr uint32_t DATA_FIXBINARY = 3;	/* binary string of fixed length */
static constexpr uint32_t DATA_BINARY = 4;	/* binary string */
static constexpr uint32_t DATA_BLOB = 5;	/* binary large object, or a TEXT type;
				if prtype & DATA_BINARY_TYPE == 0, then this is
				actually a TEXT column (or a BLOB created
				with < 4.0.14; since column prefix indexes
				came only in 4.0.14, the missing flag in BLOBs
				created before that does not cause any harm) */
static constexpr uint32_t DATA_INT = 6;	/* integer: can be any size 1 - 8 bytes */
static constexpr uint32_t DATA_SYS_CHILD = 7;	/* address of the child page in node pointer */
static constexpr uint32_t DATA_SYS = 8;	/* system column */
/* Data types >= DATA_FLOAT must be compared using the whole field, not as
binary strings */

static constexpr uint32_t DATA_FLOAT = 9;
static constexpr uint32_t DATA_DOUBLE = 10;
static constexpr uint32_t DATA_DECIMAL = 11;	/* decimal number stored as an ASCII string */
static constexpr uint32_t	DATA_VARMYSQL	= 12;	/* any charset varying length char */
static constexpr uint32_t	DATA_MYSQL = 13;	/* any charset fixed length char */
/* NOTE that 4.1.1 used DATA_MYSQL and
DATA_VARMYSQL for all character sets, and the
charset-collation for tables created with it
can also be latin1_swedish_ci */

/* DATA_POINT&DATA_VAR_POINT are for standard geometry datatype 'point' and
DATA_GEOMETRY include all other standard geometry datatypes as described in
OGC standard(line_string, polygon, multi_point, multi_polygon,
multi_line_string, geometry_collection, geometry).
Currently, geometry data is stored in the standard Well-Known Binary(WKB)
format (http://www.opengeospatial.org/standards/sfa).
We use BLOB as underlying datatype for DATA_GEOMETRY and DATA_VAR_POINT
while CHAR for DATA_POINT */
static constexpr uint32_t DATA_GEOMETRY	= 14;	/* geometry datatype of variable length */
/* The following two are disabled temporarily, we won't create them in
get_innobase_type_from_mysql_type().
again. */
static constexpr uint32_t DATA_POINT = 15;	/* geometry datatype of fixed length POINT */
static constexpr uint32_t DATA_VAR_POINT = 16;	/* geometry datatype of variable length
				POINT, used when we want to store POINT
				as BLOB internally */
static constexpr uint32_t DATA_MTYPE_MAX = 63;	/* dtype_store_for_order_and_null_size()
				requires the values are <= 63 */

static constexpr uint32_t DATA_MTYPE_CURRENT_MIN = DATA_VARCHAR;	/* minimum value of mtype */
static constexpr uint32_t DATA_MTYPE_CURRENT_MAX = DATA_VAR_POINT;	/* maximum value of mtype */

static constexpr uint32_t DATA_MBMAX = 5;

static constexpr uint32_t REC_NEW_STATUS = 3;	/* This is single byte bit-field */
static constexpr uint32_t REC_NEW_STATUS_MASK = 0x7UL;
static constexpr uint32_t REC_NEW_STATUS_SHIFT = 0;

/*-----------------------------*/
static constexpr uint32_t PAGE_N_DIR_SLOTS = 0;	/* number of slots in page directory */
static constexpr uint32_t PAGE_HEAP_TOP	= 2;	/* pointer to record heap top */
static constexpr uint32_t PAGE_N_HEAP = 4;	/* number of records in the heap,
				bit 15=flag: new-style compact page format */
static constexpr uint32_t PAGE_FREE = 6;	/* pointer to start of page free record list */
static constexpr uint32_t PAGE_GARBAGE = 8;	/* number of bytes in deleted records */
static constexpr uint32_t PAGE_LAST_INSERT = 10;	/* pointer to the last inserted record, or
				NULL if this info has been reset by a delete,
				for example */
static constexpr uint32_t PAGE_DIRECTION = 12;	/* last insert direction: PAGE_LEFT, ... */
static constexpr uint32_t PAGE_N_DIRECTION = 14;	/* number of consecutive inserts to the same
				direction */
static constexpr uint32_t PAGE_N_RECS = 16;	/* number of user records on the page */
static constexpr uint32_t PAGE_MAX_TRX_ID = 18;	/* highest id of a trx which may have modified
				a record on the page; trx_id_t; defined only
				in secondary indexes and in the insert buffer
				tree */
static constexpr uint32_t PAGE_HEADER_PRIV_END = 26;	/* end of private data structure of the page
				header which are set in a page create */
/*----*/
static constexpr uint32_t PAGE_LEVEL = 26;	/* level of the node in an index tree; the
				leaf level is the level 0.  This field should
				not be written to after page creation. */
static constexpr uint32_t PAGE_INDEX_ID = 28;	/* index id where the page belongs.
				This field should not be written to after
				page creation. */


static constexpr uint32_t DICT_MAX_FIXED_COL_LEN = 768;

static constexpr uint32_t REC_STATUS_ORDINARY = 0;
static constexpr uint32_t REC_STATUS_NODE_PTR = 1;
static constexpr uint32_t REC_STATUS_INFIMUM = 2;
static constexpr uint32_t REC_STATUS_SUPREMUM = 3;

static constexpr uint32_t REC_OFFS_HEADER_SIZE = 4;

static constexpr uint32_t ULINT_UNDEFINED = UINT32_MAX;

/* Compact flag ORed to the extra size returned by rec_get_offsets() */
static constexpr uint32_t REC_OFFS_COMPACT = ((uint32_t) 1 << 31);
/* SQL NULL flag in offsets returned by rec_get_offsets() */
static constexpr uint32_t REC_OFFS_SQL_NULL = ((uint32_t) 1 << 31);
/* External flag in offsets returned by rec_get_offsets() */
static constexpr uint32_t REC_OFFS_EXTERNAL = ((uint32_t) 1 << 30);
/* Mask for offsets returned by rec_get_offsets() */
static constexpr uint32_t REC_OFFS_MASK = (REC_OFFS_EXTERNAL - 1);

static constexpr uint32_t REC_NEXT = 2;
static constexpr uint32_t REC_NEXT_MASK = 0xFFFFUL;
static constexpr uint32_t REC_NEXT_SHIFT = 0;


/* The offset of heap_no in a compact record */
static constexpr uint32_t  REC_NEW_HEAP_NO = 4;
/* The shift of heap_no in a compact record.
The status is stored in the low-order bits. */
static constexpr uint32_t REC_HEAP_NO_SHIFT = 3;

/* Length of a B-tree node pointer, in bytes */
static constexpr uint32_t REC_NODE_PTR_SIZE = 4;

static constexpr uint32_t REC_HEAP_NO_MASK = 0xFFF8UL;

static constexpr uint32_t PAGE_DIR_SLOT_MAX_N_OWNED = 8;
static constexpr uint32_t	PAGE_DIR_SLOT_MIN_N_OWNED = 4;

static constexpr uint32_t	PAGE_LEFT = 1;
static constexpr uint32_t	PAGE_RIGHT = 2;
static constexpr uint32_t	PAGE_SAME_REC	= 3;
static constexpr uint32_t	PAGE_SAME_PAGE = 4;
static constexpr uint32_t	PAGE_NO_DIRECTION = 5;

enum {
  /** do no undo logging */
  BTR_NO_UNDO_LOG_FLAG = 1,
  /** do no record lock checking */
  BTR_NO_LOCKING_FLAG = 2,
  /** sys fields_ will be found in the update vector or inserted
  entry */
  BTR_KEEP_SYS_FLAG = 4,
  /** btr_cur_pessimistic_update() must keep cursor position
  when moving columns to big_rec */
  BTR_KEEP_POS_FLAG = 8,
  /** the caller is creating the index or wants to bypass the
  index->info.online creation log */
  BTR_CREATE_FLAG = 16,
  /** the caller of btr_cur_optimistic_update() or
  btr_cur_update_in_place() will take care of
  updating IBUF_BITMAP_FREE */
  BTR_KEEP_IBUF_BITMAP = 32
};

static constexpr uint32_t	UNIV_SQL_NULL = 0xFFFFFFFF;

static constexpr uint32_t REC_MAX_N_FIELDS = 1024 - 1;

static constexpr uint32_t REC_OLD_SHORT = 3;	/* This is single byte bit-field */
static constexpr uint32_t REC_OLD_SHORT_MASK = 0x1UL;
static constexpr uint32_t REC_OLD_SHORT_SHIFT = 0;

static constexpr uint32_t REC_OLD_N_FIELDS = 4;
static constexpr uint32_t REC_OLD_N_FIELDS_MASK = 0x7FEUL;
static constexpr uint32_t REC_OLD_N_FIELDS_SHIFT = 1;

static constexpr uint32_t REC_N_OLD_EXTRA_BYTES = 6;

/** SQL null flag in a 1-byte offset of ROW_FORMAT=REDUNDANT records */
static constexpr uint32_t REC_1BYTE_SQL_NULL_MASK = 0x80UL;
/** SQL null flag in a 2-byte offset of ROW_FORMAT=REDUNDANT records */
static constexpr uint32_t REC_2BYTE_SQL_NULL_MASK =	0x8000UL;

/** In a 2-byte offset of ROW_FORMAT=REDUNDANT records, the second most
significant bit denotes that the tail of a field is stored off-page. */
static constexpr uint32_t REC_2BYTE_EXTERN_MASK = 0x4000UL;

static constexpr uint32_t BUF_NO_CHECKSUM_MAGIC = 0xDEADBEEFUL;

static constexpr uint32_t FIL_PAGE_SPACE_OR_CHKSUM = 0;

static constexpr uint32_t IBUF_BITMAP = PAGE_DATA;

static constexpr uint32_t OS_FILE_LOG_BLOCK_SIZE = 512;

}
