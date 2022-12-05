#ifndef _STORPU_FILE_H_
#define _STORPU_FILE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FD_HOST_MEM   (-2)
#define FD_SCRATCHPAD (-3)
#define FD_FLASH (1)
ssize_t spu_read(int fd, void *buf, size_t count, unsigned long offset);
ssize_t spu_write(int fd, const void *buf, size_t count, unsigned long offset);

#ifdef __cplusplus
}
#endif


#endif
