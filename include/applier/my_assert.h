#pragma once
#include "storpu/storpu.h"
#ifdef DEBUG

#define MY_ASSERT(condition)                                                    \
  do {                                                                          \
       if (!(condition)) {                                                      \
         spu_printf("assert failed in file: %s, line: %d", __FILE__, __LINE__); \
       }                                                                        \
  } while(0)
#else

#define MY_ASSERT(condition)                                                    \
  do {} while(0)
#endif