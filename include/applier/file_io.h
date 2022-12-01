#pragma once
#include "storpu/storpu.h"
#include "storpu/file.h"
#include "config.h"
namespace Lemon {

ssize_t flash_read(unsigned int lpa, unsigned int n_pages, void *buf);

ssize_t flash_write(unsigned int lpa, unsigned int n_pages, void *buf);

}