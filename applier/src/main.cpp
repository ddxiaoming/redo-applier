#include "applier/apply.h"
void test() {
  Lemon::BufferPool buffer_pool;
  Lemon::ApplySystem apply_system;
  apply_system.SetBufferPool(&buffer_pool);
  while (1) {
    if (!apply_system.PopulateHashMap()) {
      break;
    }
    apply_system.ApplyHashLogs();
  }
}