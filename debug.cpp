#include <fstream>
#include <iostream>
int main() {
  std::ifstream ifs1("/home/lemon/mysql/mysql_debug", std::ios::binary);
  std::ifstream ifs2("/home/lemon/mysql/redo_applier_debug", std::ios::binary);
  unsigned char buf1[16 * 1024], buf2[16 * 1024];
  int i = 1;
  while (ifs1.read((char *)buf1, 16 * 1024) && ifs2.read((char *)buf2, 16 * 1024)) {
    for (int j = 38; j < 16 * 1024 - 8; ++j) {
      if (buf1[j] != buf2[j]) {
        std::cout << "i = " << i << ", j = " << j << std::endl;
        return -1;
      }
    }
    std::cout << "i = " << i << std::endl;
    ++i;
  }
}