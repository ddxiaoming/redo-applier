#pragma once
#include "storpu/storpu.h"
#include "storpu/file.h"
#include "config.h"
namespace Lemon {

ssize_t flash_read(unsigned int lpa, unsigned int n_pages, void *buf);

ssize_t flash_write(unsigned int lpa, unsigned int n_pages, void *buf);

// interface for host
// host invokes this function to read data from ssd
unsigned long data_read(unsigned long args_ptr);
// host invokes this function to write data to ssd
unsigned long data_write(unsigned long args_ptr);

}