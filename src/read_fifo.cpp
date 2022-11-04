#include <iostream>
#include <fcntl.h>
#include <csignal>
#include <fstream>
#include <chrono>
#include "config.h"
int main() {
//  int fifo_mysql = open("/home/lemon/fifo_mysql", O_RDONLY);
//  if (fifo_mysql == -1) {
//    std::cout << "open fifo_mysql failed." << std::endl;
//    exit(1);
//  }
//  int fifo_redo_applier = open("/home/lemon/fifo_redo_applier", O_RDONLY);
//  if (fifo_redo_applier == -1) {
//    std::cout << "open fifo_redo_applier failed." << std::endl;
//    exit(1);
//  }
//
//  unsigned char buf1[Lemon::DATA_PAGE_SIZE];
//  unsigned char buf2[Lemon::DATA_PAGE_SIZE];
//  int i = 0;
//  while (1) {
//    long read_len = 0;
//    while (read_len != (long) Lemon::DATA_PAGE_SIZE) {
//      long ret = read(fifo_mysql, buf1 + read_len, Lemon::DATA_PAGE_SIZE - read_len);
//      if (ret == -1) {
//        std::cout << "read from fifo_redo_applier failed." << std::endl;
//        exit(1);
//      }
//      read_len += ret;
//    }
//
//    read_len = 0;
//    while (read_len != (long) Lemon::DATA_PAGE_SIZE) {
//      long ret = read(fifo_redo_applier, buf2 + read_len, Lemon::DATA_PAGE_SIZE - read_len);
//      if (ret == -1) {
//        std::cout << "read from fifo_redo_applier failed." << std::endl;
//        exit(1);
//      }
//      read_len += ret;
//    }
//
//    //compare int j = 38; j < Lemon::DATA_PAGE_SIZE - 8; ++j
//    i++;
//    for (int j = 38; j < Lemon::DATA_PAGE_SIZE - 8; ++j) {
//      if (buf1[j] != buf2[j]) {
//        std::cout << "i = " << i << ", j = " << j << std::endl;
//        return -1;
//      }
//
//    }
//    std::cout << "success " << i << std::endl;
//  }

  auto t1 = std::chrono::steady_clock::now();
  const unsigned long long MAX_FILE_SIZE = 2ULL * 1024ULL * 1024ULL * 1024ULL;
  const unsigned long long PAGE_SIZE = 16ULL * 1024ULL;
  char buf[PAGE_SIZE];
  std::ifstream ifs("/home/lemon/mysql/data/ib_logfile0");
  auto t2 = std::chrono::steady_clock::now();
  int range = MAX_FILE_SIZE / PAGE_SIZE;
  for (int i = 0; i < range; ++i) {
    ifs.seekg(i * PAGE_SIZE);
    ifs.read(buf, PAGE_SIZE);
  }
  std::cout << "Elapsed time: " << (t2 - t1).count() << "ns." << std::endl;
}