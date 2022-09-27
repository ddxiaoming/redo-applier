#pragma once
#include <vector>
#include <string>
#include "config.h"
namespace Lemon {

bool TravelDirectory(const std::string &dir_path, const std::string &suffix, std::vector<std::string> &files);

const char* GetLogString(LOG_TYPE type);

inline uint8_t mach_read_from_1(const byte*	b) {
  return static_cast<uint8_t>(b[0]);
}

inline uint16_t mach_read_from_2(const byte* b) {
  return (static_cast<uint16_t>(b[0]) << 8) | static_cast<uint16_t>(b[1]);
}

inline uint32_t mach_read_from_3(const byte* b) {
  return (static_cast<uint32_t>(b[0]) << 16)
  | (static_cast<uint32_t>(b[1]) << 8)
  | static_cast<uint32_t>(b[2]);
}

inline uint32_t mach_read_from_4(const byte*	b) {
  return (static_cast<uint32_t>(b[0]) << 24)
  | (static_cast<uint32_t>(b[1]) << 16)
  | (static_cast<uint32_t>(b[2]) << 8)
  | static_cast<uint32_t>(b[3]);
}

inline uint64_t mach_read_from_8(const byte*	b) {
  uint64_t	u64;

  u64 = mach_read_from_4(b);
  u64 <<= 32;
  u64 |= mach_read_from_4(b + 4);

  return u64;
}

/** Read a 32-bit integer in a compressed form.
@param[in,out]	ptr	pointer to memory where to read;
advanced by the number of bytes consumed, or set nullptr if out of space
@param[in]	end_ptr	end of the buffer
@return unsigned value */
uint32_t mach_parse_compressed(const byte **ptr, const byte* end_ptr);

/** Read a 32-bit integer in a compressed form.
@param[in,out]	b	pointer to memory where to read;
advanced by the number of bytes consumed
@return unsigned value */
uint32_t mach_read_next_compressed(const byte**	b);


/** Read a 64-bit integer in a compressed form.
@param[in,out]	ptr	pointer to memory where to read;
advanced by the number of bytes consumed, or set nullptr if out of space
@param[in]	end_ptr	end of the buffer
@return unsigned value */
uint64_t mach_u64_parse_compressed(const byte**	ptr, const byte*	end_ptr);

}