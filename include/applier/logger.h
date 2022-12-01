#pragma once

#ifdef DEBUG

#define LOG_DEBUG(message, ...)             \
  do {                                      \
       spu_printf(message, ##__VA_ARGS__);  \
  } while(0)
#else

#define LOG_DEBUG(message, ...)             \
  do {} while(0)
#endif