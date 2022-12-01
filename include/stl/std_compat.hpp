#pragma once
#include <cstdlib>
#include "storpu/storpu.h"
namespace frg {

struct stl_allocator {
  static void *allocate(size_t size) {
    return ::malloc(size);
    //		return operator new(size);
  }

  static void deallocate(void *ptr, size_t size) {
    ::free(ptr);
    //		operator delete(ptr, size);
  }

  static void free(void *ptr) {
    ::free(ptr);
    //		operator delete(ptr);
  }
};

} // namespace frg
