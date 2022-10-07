#include "apply.h"
using namespace Lemon;
int main() {
  ApplySystem applySystem;
  for (int i = 0; i < 256; ++i) {
    applySystem.PopulateHashMap();
  }
}