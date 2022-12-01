#pragma once
#include <cstdlib>
namespace frg {
class DefaultAllocator {
public:
  DefaultAllocator() = default;
  static void *allocate(size_t size) {
    return malloc(size);
  }
  static void deallocate(void *mem) {
    free(mem);
  }
};
}
