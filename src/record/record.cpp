#include "record.h"
#include <cassert>
#include <cstring>
#include "utility.h"
namespace Lemon {

uint32_t
rec_get_bit_field_1(const byte*	rec, uint32_t offs, uint32_t mask, uint32_t shift) {
  return((mach_read_from_1(rec - offs) & mask) >> shift);
}

bool rec_info_bits_valid(uint32_t bits) {
  return(0 == (bits & ~(REC_INFO_DELETED_FLAG | REC_INFO_MIN_REC_FLAG)));
}

void rec_set_bit_field_1(byte*	rec, uint32_t	val, uint32_t	offs, uint32_t mask, uint32_t shift) {
  mach_write_to_1(rec - offs,
                  (mach_read_from_1(rec - offs) & ~mask)
                  | (val << shift));
}
/******************************************************//**
Sets a bit field within 2 bytes. */
void
rec_set_bit_field_2(
/*================*/
    byte*	rec,	/*!< in: pointer to record origin */
    uint32_t 	val,	/*!< in: value to set */
    uint32_t	offs,	/*!< in: offset from the origin down */
    uint32_t	mask,	/*!< in: mask used to filter bits */
    uint32_t	shift)	/*!< in: shift right applied after masking */
{
  assert(rec);
  assert(mask > 0xFFUL);
  assert(mask <= 0xFFFFUL);
  assert((mask >> shift) & 1);
  assert(0 == ((mask >> shift) & ((mask >> shift) + 1)));
  assert(((mask >> shift) << shift) == mask);
  assert(((val << shift) & mask) == (val << shift));

  mach_write_to_2(rec - offs,
                  (mach_read_from_2(rec - offs) & ~mask)
                  | (val << shift));
}

uint32_t rec_get_info_bits(const byte* rec, bool comp) {
  const uint32_t	val = rec_get_bit_field_1(rec, comp ? REC_NEW_INFO_BITS : REC_OLD_INFO_BITS,
                                            REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
  assert(rec_info_bits_valid(val));
  return(val);
}


void rec_set_info_bits_new(byte*	rec, uint32_t bits) {
  assert(rec_info_bits_valid(bits));
  rec_set_bit_field_1(rec, bits, REC_NEW_INFO_BITS,
                      REC_INFO_BITS_MASK, REC_INFO_BITS_SHIFT);
}


/******************************************************//**
The following function retrieves the status bits of a new-style record.
@return status bits */
uint32_t
rec_get_status(
/*===========*/
    const byte*	rec)	/*!< in: physical record */
{
  uint32_t ret;

  ret = rec_get_bit_field_1(rec, REC_NEW_STATUS,
                            REC_NEW_STATUS_MASK, REC_NEW_STATUS_SHIFT);

  return(ret);
}


/******************************************************//**
The following function is used to retrieve the info and status
bits of a record.  (Only compact records have status bits.)
@return info bits */
uint32_t
rec_get_info_and_status_bits(
    const byte*	rec	/*!< in: physical record */
)
{
  uint32_t bits = 0;
  bits = rec_get_info_bits(rec, true) | rec_get_status(rec);
  return(bits);
}


/******************************************************//**
The following function is used to set the status bits of a new-style record. */
void
rec_set_status(
/*===========*/
    byte*	rec,	/*!< in/out: physical record */
    uint32_t bits)	/*!< in: info bits */
{
  rec_set_bit_field_1(rec, bits, REC_NEW_STATUS,
                      REC_NEW_STATUS_MASK, REC_NEW_STATUS_SHIFT);
}


/******************************************************//**
The following function is used to set the info and status
bits of a record.  (Only compact records have status bits.) */
void
rec_set_info_and_status_bits(
/*=========================*/
    byte*	rec,	/*!< in/out: physical record */
    uint32_t bits)	/*!< in: info bits */
{
  rec_set_status(rec, bits & REC_NEW_STATUS_MASK);
  rec_set_info_bits_new(rec, bits & ~REC_NEW_STATUS_MASK);
}

/******************************************************//**
Gets a bit field from within 2 bytes. */
uint32_t
rec_get_bit_field_2(
/*================*/
    const byte*	rec,	/*!< in: pointer to record origin */
    uint32_t offs,	/*!< in: offset from the origin down */
    uint32_t mask,	/*!< in: mask used to filter bits */
    uint32_t shift)	/*!< in: shift right applied after masking */
{
  return((mach_read_from_2(rec - offs) & mask) >> shift);
}

/******************************************************//**
The following function is used to get the order number
of a new-style record in the heap of the index page.
@return heap order number */
uint32_t
rec_get_heap_no_new(
/*================*/
    const byte*	rec)	/*!< in: physical record */
{
  return(rec_get_bit_field_2(rec, REC_NEW_HEAP_NO,
                             REC_HEAP_NO_MASK, REC_HEAP_NO_SHIFT));
}



/******************************************************//**
The following function is used to get the pointer of the next chained record
on the same page.
@return pointer to the next chained record, or NULL if none */
const byte*
rec_get_next_ptr_const(
/*===================*/
    const byte*	rec)	/*!< in: physical record */

{
  uint32_t 	field_value;

  field_value = mach_read_from_2(rec - REC_NEXT);

  if (field_value == 0) {

    return nullptr;
  }

  // TODO 可能有问题的地方
  return rec + field_value;
}

/******************************************************//**
The following function is used to get the pointer of the next chained record
on the same page.
@return pointer to the next chained record, or NULL if none */
byte*
rec_get_next_ptr(
/*=============*/
    byte*	rec) /*!< in: physical record */
{
return(const_cast<byte*>(rec_get_next_ptr_const(rec)));
}

byte*
rec_copy(void* buf, const RecordInfo &rec_info)
{
  uint32_t 	extra_len;
  uint32_t	data_len;

  byte *rec_ptr = rec_info.GetRecPtr();
  assert(rec_ptr != nullptr);
  assert(buf != nullptr);

  extra_len = rec_info.GetExtraSize();
  data_len = rec_info.GetDataSize();

  std::memcpy(buf, rec_ptr - extra_len, extra_len + data_len);

  return((byte*) buf + extra_len);
}

uint32_t rec_get_next_offs(const byte* page, const byte *rec) {
  uint32_t field_value;
  field_value = mach_read_from_2(rec - REC_NEXT);
  if (field_value == 0) {
    return(0);
  }
  // TODO 可能有问题的地方
  return (rec + field_value - page);
}

/******************************************************//**
The following function is used to set the next record offset field
of a new-style record. */
void
rec_set_next_offs_new(
/*==================*/
    const byte* page, /* relevant page */
    byte*	rec,	/*!< in/out: new-style physical record */
    uint32_t 	next)	/*!< in: offset of the next record */
{
  uint32_t	field_value;

  assert(rec);
  assert(DATA_PAGE_SIZE > next);

  if (!next) {
    field_value = 0;
  } else {

    // TODO 可能有问题的地方
    field_value = (uint32_t)
        ((int32_t) next
         - (int32_t) (rec - page));
    field_value &= REC_NEXT_MASK;
  }

  mach_write_to_2(rec - REC_NEXT, field_value);
}

/******************************************************//**
The following function is used to set the number of owned records. */
void rec_set_n_owned_new(
/*================*/
    byte*		rec,
    uint32_t n_owned)/*!< in: the number of owned */
{
  rec_set_bit_field_1(rec, n_owned, REC_NEW_N_OWNED,
                      REC_N_OWNED_MASK, REC_N_OWNED_SHIFT);
}

/******************************************************//**
The following function is used to set the heap number
field in a new-style record. */
void
rec_set_heap_no_new(
/*================*/
    byte*	rec,	/*!< in/out: physical record */
    uint32_t heap_no)/*!< in: the heap number */
{
  rec_set_bit_field_2(rec, heap_no, REC_NEW_HEAP_NO,
                      REC_HEAP_NO_MASK, REC_HEAP_NO_SHIFT);
}

/******************************************************//**
The following function is used to set the deleted bit. */
void
rec_set_deleted_flag_new(
/*=====================*/
    byte* rec,	/*!< in/out: new-style physical record */
    uint32_t flag)	/*!< in: nonzero if delete marked */
{
  uint32_t val;

  val = rec_get_info_bits(rec, true);

  if (flag) {
    val |= REC_INFO_DELETED_FLAG;
  } else {
    val &= ~REC_INFO_DELETED_FLAG;
  }

  rec_set_info_bits_new(rec, val);
}

/************************************************************//**
The following function is used to get an offset to the nth
data field in a record.
@return offset from the origin of rec */
uint32_t
rec_get_nth_field_offs(
/*===================*/
    const RecordInfo &rec_info,/*!< in: array returned by rec_get_offsets() */
    uint32_t		n,	/*!< in: index of the field */
    uint32_t*		len)	/*!< out: length of the field; UNIV_SQL_NULL
				if SQL null */
{
  uint32_t	offs;
  uint32_t	length;
  assert(len);

  if (n == 0) {
    offs = 0;
  } else {
    offs = rec_info.GetNOffset(REC_OFFS_HEADER_SIZE + n) & REC_OFFS_MASK;
  }

  length = rec_info.GetNOffset(REC_OFFS_HEADER_SIZE + n + 1);

  if (length & REC_OFFS_SQL_NULL) {
    length = UNIV_SQL_NULL;
  } else {
    length &= REC_OFFS_MASK;
    length -= offs;
  }

  *len = length;
  return(offs);
}

byte *rec_get_nth_field(byte *rec, const RecordInfo &rec_info, uint32_t	n, uint32_t* len) {
  return rec + rec_get_nth_field_offs(rec_info, n, len);
}
}