#include <cstdio>
#include "applier/file_io.h"

namespace Lemon {

ssize_t flash_read(unsigned int lpa, unsigned int n_pages, void *buf) {
  return spu_read(FD_FLASH, buf, FLASH_PAGE_SIZE * n_pages, lpa * FLASH_PAGE_SIZE);
}

ssize_t flash_write(unsigned int lpa, unsigned int n_pages, void *buf) {
  return spu_write(FD_FLASH, buf, FLASH_PAGE_SIZE * n_pages, lpa * FLASH_PAGE_SIZE);
}

unsigned long data_write(unsigned long args_ptr) {

  struct {
    unsigned long host_addr;
    unsigned long flash_page_id;
    unsigned long n_pages;
  } args {};

  // read args from host memory
  spu_read(FD_SCRATCHPAD, &args, sizeof(args), args_ptr);

  // allocate a buffer in ssd
  void *io_buf = mmap(nullptr,
                     FLASH_PAGE_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG,
                     -1,
                     0);

  for (int i = 0; i < args.n_pages; ++i) {
    spu_read(FD_HOST_MEM, io_buf,
             FLASH_PAGE_SIZE,
             args.host_addr + i * FLASH_PAGE_SIZE);
    spu_write(FD_FLASH, io_buf,
              FLASH_PAGE_SIZE,
              (args.flash_page_id + i) * FLASH_PAGE_SIZE);
  }
  munmap(io_buf, FLASH_PAGE_SIZE);
  sync();
  return 0;
}

unsigned long data_read(unsigned long args_ptr) {

  struct {
    unsigned long host_addr;
    unsigned long flash_page_id;
    unsigned long page_num;
  } args{};

  // read args from host memory
  spu_read(FD_SCRATCHPAD, &args, sizeof(args), args_ptr);

  // allocate a buffer in ssd
  void *io_buf = mmap(nullptr,
                     FLASH_PAGE_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG,
                     -1,
                     0);

  for (int i = 0; i < args.page_num; ++i) {
    spu_read(FD_FLASH, io_buf,
             FLASH_PAGE_SIZE,
             (args.flash_page_id + i) * FLASH_PAGE_SIZE);
    spu_write(FD_HOST_MEM, io_buf,
              FLASH_PAGE_SIZE,
              args.host_addr + i * FLASH_PAGE_SIZE);
  }
  munmap(io_buf, FLASH_PAGE_SIZE);
  return 0;
}


}