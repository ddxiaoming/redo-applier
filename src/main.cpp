#include "apply.h"
#include <iostream>
#include <fstream>
#include <fmt/format.h>
using namespace Lemon;
void CompareLog() {
  std::ifstream standard_ifs("/home/lemon/redolog.txt", std::ios::in);
  std::ifstream my_ifs("/home/lemon/redolog2.txt", std::ios::in);
  // 一行一行的比较
  int line_number = 1;
  std::string standard_line;
  std::string my_line;
  while (true) {
    std::getline(standard_ifs, standard_line);
    std::getline(my_ifs, my_line);
    std::string standard_type = standard_line.substr(0, standard_line.find(','));
    std::string my_type = my_line.substr(0, my_line.find(','));
    if (my_type == "type = MLOG_CHECKPOINT" && standard_type == "type = MLOG_CHECKPOINT") {
      line_number++;
      continue;
    }
    if (my_type == "type == MLOG_COMP_REC_INSERT") {
//      fmt::print("line:{}, log:{}\n", line_number, my_line);
      break;
    }
    if (standard_line != my_line) {
      break;
    }
    line_number++;
  }
  fmt::print("line:{}\n", line_number);
  fmt::print("standard:{}\n", standard_line);
  fmt::print("my:{}\n", my_line);
}
int main() {
  ApplySystem applySystem;
  applySystem.SetSaveLogs(false);
//  CompareLog();
  while (1) {
    applySystem.PopulateHashMap();
    applySystem.ApplyHashLogs();
  }
}