#include <stddef.h>
#include <cstdio>
#include "applier/file_io.h"

namespace Lemon {

ssize_t flash_read(unsigned int lpa, unsigned int n_pages, void *buf) {
  return spu_read(FD_FLASH, buf, FLASH_PAGE_SIZE * n_pages, lpa * FLASH_PAGE_SIZE);
}

ssize_t flash_write(unsigned int lpa, unsigned int n_pages, void *buf) {
  return spu_write(FD_FLASH, buf, FLASH_PAGE_SIZE * n_pages, lpa * FLASH_PAGE_SIZE);
}




}