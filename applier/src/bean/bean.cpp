#include <cstdlib>
#include <cstring>
#include "applier/bean.h"
#include "applier/record.h"
#include "storpu/storpu.h"
#include "stl/macros.hpp"
#include "applier/my_assert.h"
#include "applier/logger.h"

namespace Lemon {
void RecordInfo::AddField(uint32_t main_type, uint32_t precise_type, uint32_t length) {
  // 构造fixed_length
  uint32_t fixed_len = 0;
  if (main_type == DATA_FIXBINARY) {
    fixed_len = length;
  } else if (main_type == DATA_BINARY) {
    fixed_len = 0;
  }

  if (fixed_len > DICT_MAX_FIXED_COL_LEN) {
    fixed_len = 0;
  }

  // 更新 n_nullable字段
  if (!(precise_type & DATA_NOT_NULL)) {
    n_nullable_++;
  }
//  data_size_ += length;

  int i = static_cast<int>(fields_.size());


  // TRX_ID 和 ROLL_PTR列特殊处理
  if (n_fields_ != n_unique_ && i == DATA_TRX_ID - 1 + n_unique_) {
    MY_ASSERT(length == DATA_TRX_ID_LEN);
    main_type = DATA_SYS;
    precise_type = DATA_TRX_ID | DATA_NOT_NULL;
  }

  if (n_fields_ != n_unique_ && i == DATA_ROLL_PTR - 1 + n_unique_) {
    MY_ASSERT(length == DATA_ROLL_PTR_LEN);
    main_type = DATA_SYS;
    precise_type = DATA_ROLL_PTR | DATA_NOT_NULL;
  }

  fields_.emplace_back(main_type, precise_type, length, fixed_len);
}
static bool DATA_BIG_COL(const FieldInfo &col) {
  return (col.length_ > 255)
  || (col.main_type_ == DATA_BLOB
  || col.main_type_ == DATA_VAR_POINT
  || col.main_type_  == DATA_GEOMETRY);
}


void RecordInfo::CalculateOffsets(uint32_t max_n) {
  offsets_.clear();

  uint32_t n = 0, size = 0;
  MY_ASSERT(rec_ptr_ != nullptr);

  uint32_t status = rec_get_status(rec_ptr_);
  switch (status) {
    case REC_STATUS_ORDINARY:
      n = n_fields_;
      break;
    case REC_STATUS_NODE_PTR:
      if (index_type_ & DICT_CLUSTERED) {
        n = n_unique_ + 1;
      } else {
        n = n_fields_ + 1;
      }
      break;
    case REC_STATUS_INFIMUM:
    case REC_STATUS_SUPREMUM:
      n = 1;
      break;
    default:
      // error
      LOG_DEBUG("error when rec_get_status\n");
      return;
  }

  if (max_n < n) {
    n = max_n;
  }

  size = n + (1 + REC_OFFS_HEADER_SIZE);

  // 分配内存
  offsets_.resize(size);

  offsets_[1] = n;

  // offsets[2]和offsets[3]用不到

  const byte*	nulls;
  const byte*	lens;
  uint32_t null_mask;
  uint32_t offs = 0;
  uint32_t i = 0;
  uint32_t n_node_ptr_field = ULINT_UNDEFINED;

  switch (status) {
    case REC_STATUS_INFIMUM:
    case REC_STATUS_SUPREMUM:
      /* the field is 8 bytes long */
      offsets_[0 + REC_OFFS_HEADER_SIZE] = REC_N_NEW_EXTRA_BYTES | REC_OFFS_COMPACT;
      offsets_[1 + REC_OFFS_HEADER_SIZE] = 8;
      return;
    case REC_STATUS_NODE_PTR:
      if (index_type_ & DICT_CLUSTERED) {
        n_node_ptr_field = n_unique_;
      } else {
        n_node_ptr_field = n_fields_;
      }
      break;
    case REC_STATUS_ORDINARY:
      InitOffsetsCompOrdinary();
      return;
    default:
      return;
  }

  nulls = rec_ptr_ - (REC_N_NEW_EXTRA_BYTES + 1);
  lens = nulls - ((n_nullable_ + 7) / 8);
  offs = 0;
  null_mask = 1;

  /* read the lengths of fields_ 0..n */
  do {
    uint32_t len;
    if (i == n_node_ptr_field) {
      len = offs += REC_NODE_PTR_SIZE;
      goto resolved;
    }

    if (!(fields_[i].precise_type_ & DATA_NOT_NULL)) {
      /* nullable field => read the null flag */

      if ((!(byte) null_mask)) {
        nulls--;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        /* No length is stored for NULL fields_.
        We do not advance offs, and we set
        the length to zero and enable the
        SQL NULL flag in offsets[]. */
        len = offs | REC_OFFS_SQL_NULL;
        goto resolved;
      }
      null_mask <<= 1;
    }

    if ((!fields_[i].fixed_length_)) {

      /* Variable-length field: read the length */
      len = *lens--;
      /* If the maximum length of the field
      is up to 255 bytes, the actual length
      is always stored in one byte. If the
      maximum length is more than 255 bytes,
      the actual length is stored in one
      byte for 0..127.  The length will be
      encoded in two bytes when it is 128 or
      more, or when the field is stored
      externally. */
      if (DATA_BIG_COL(fields_[i])) {
        if (len & 0x80) {
          /* 1exxxxxxx xxxxxxxx */

          len <<= 8;
          len |= *lens--;

          /* B-tree node pointers
          must not contain externally
          stored columns.  Thus
          the "e" flag must be 0. */
          MY_ASSERT(!(len & 0x4000));
          offs += len & 0x3fff;
          len = offs;

          goto resolved;
        }
      }

      len = offs += len;
    } else {
      len = offs += fields_[i].fixed_length_;
    }
resolved:
    offsets_[REC_OFFS_HEADER_SIZE + i + 1] = len;
  } while (++i < offsets_[1]);

  offsets_[REC_OFFS_HEADER_SIZE] = (rec_ptr_ - (lens + 1)) | REC_OFFS_COMPACT;

}

uint32_t RecordInfo::GetExtraSize() const {
  MY_ASSERT(offsets_.size() > REC_OFFS_HEADER_SIZE);
  return offsets_[REC_OFFS_HEADER_SIZE] & ~(REC_OFFS_COMPACT | REC_OFFS_EXTERNAL);
}

void RecordInfo::InitOffsetsCompOrdinary() {
  uint32_t i = 0;
  uint32_t offs = 0;
  uint32_t any_ext = 0;
  uint32_t n_null = n_nullable_; // 有几列可以为null
  const byte*	nulls = rec_ptr_ - (1 + REC_N_NEW_EXTRA_BYTES); // null值列表的末端地址

  // 如何计算出来NULL值列表需要多少位来存储？这里就是答案
  const byte*	lens = nulls - ((n_null + 7) / 8); // 变长字段长度列表的末端地址
  uint32_t null_mask = 1;

  /* read the lengths of fields_ 0..n */
  do {
    uint32_t len;

    if (!(fields_[i].precise_type_ & DATA_NOT_NULL)) {
      MY_ASSERT(n_null--);
      /* nullable field => read the null flag */
      if (!(byte) null_mask) {
        nulls--;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        /* No length is stored for NULL fields_.
        We do not advance offs, and we set
        the length to zero and enable the
        SQL NULL flag in offsets[]. */
        len = offs | REC_OFFS_SQL_NULL;
        goto resolved;
      }
      null_mask <<= 1;
    }

    // 怎么找到一条rec的开头？这就是答案
    if (!fields_[i].fixed_length_) {
      /* Variable-length field: read the length */
      len = *lens--;
      /* If the maximum length of the field is up
      to 255 bytes, the actual length is always
      stored in one byte. If the maximum length is
      more than 255 bytes, the actual length is
      stored in one byte for 0..127.  The length
      will be encoded in two bytes when it is 128 or
      more, or when the field is stored externally. */
      if (DATA_BIG_COL(fields_[i])) {
        if (len & 0x80) {
          /* 1exxxxxxx xxxxxxxx */
          len <<= 8;
          len |= *lens--;

          offs += len & 0x3fff;
          if ((len & 0x4000)) {
            any_ext = REC_OFFS_EXTERNAL;
            len = offs | REC_OFFS_EXTERNAL;
          } else {
            len = offs;
          }

          goto resolved;
        }
      }

      len = offs += len;
    } else {
      len = offs += fields_[i].fixed_length_;
    }
    resolved:
    offsets_[REC_OFFS_HEADER_SIZE + i + 1] = len;
  } while (++i < offsets_[1]);

  offsets_[REC_OFFS_HEADER_SIZE] = (rec_ptr_ - (lens + 1)) | REC_OFFS_COMPACT | any_ext;
}

uint32_t RecordInfo::GetDataSize() const {
  MY_ASSERT(offsets_.size() > REC_OFFS_HEADER_SIZE);
  MY_ASSERT(offsets_.size() > REC_OFFS_HEADER_SIZE + offsets_[1]);
  return offsets_[REC_OFFS_HEADER_SIZE + offsets_[1]] & REC_OFFS_MASK;
}

uint32_t RecordInfo::GetNOffset(uint32_t n) const {
  FRG_ASSERT(offsets_.size() > n);
  return offsets_[n];
}

void UpdateInfo::UpdateFieldInfo::CopyData(const byte *source, uint32_t len) {
  if (data_ != nullptr) {
    free(data_);
  }
  data_ = (byte*) malloc(len);
  len_ = len;
  memcpy(data_, source, len);
}

void UpdateInfo::UpdateFieldInfo::ResetData() {
  if (data_ != nullptr) {
    free(data_);
  }
  data_ = nullptr;
  len_ = UNIV_SQL_NULL;
}
}
