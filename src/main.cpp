#include "apply.h"
#include <iostream>
#include <fstream>
#include <fmt/format.h>
#include <unordered_map>
using namespace Lemon;
void CompareLog() {
  std::ifstream standard_ifs("/home/lemon/mysql/parsed_logs/redolog.txt", std::ios::in);
  std::ifstream my_ifs("/home/lemon/mysql/parsed_logs/log_summary.txt", std::ios::in);
  // 一行一行的比较
  int line_number = 1;
  std::string standard_line;
  std::string my_line;
  while (std::getline(standard_ifs, standard_line) && std::getline(my_ifs, my_line)) {
    int standard_pos = static_cast<int>(standard_line.find("type = "));
    int my_pos = static_cast<int>(my_line.find("type = "));
    std::string standard_type = standard_line.substr(standard_pos, standard_line.find(',', standard_pos) - standard_pos);
    std::string my_type = my_line.substr(my_pos, my_line.find(',', my_pos) - my_pos);
    if (my_type == "type = MLOG_CHECKPOINT" && standard_type == "type = MLOG_CHECKPOINT") {
      line_number++;
      continue;
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

void GetLogType() {

  std::unordered_map<std::string, int> log_type_set;
  for (int i = 1; i <= 20; ++i) {
    std::string filename("/home/lemon/mysql/parsed_logs/sbtest");
    filename += std::to_string(i);
    filename += "_log.txt";
//    std::cout << filename << std::endl;
    std::ifstream ifs(filename);
    std::string log_str;
    while (std::getline(ifs, log_str)) {
      std::string log_type;
      for (int j = 0; j < log_str.size(); ++j) {
        if (log_str.substr(i, 4) == "type") {
          int pos = static_cast<int>(log_str.find(',', i));
          log_type_set[log_str.substr(i + 7, pos - i - 7)]++;
        }
      }
    }
    ifs.close();
  }
  for (const auto &item: log_type_set) {
    std::cout << item.first << " " << item.second << std::endl;
  }
}

void test() {
  std::ifstream ifs1("/home/lemon/mysql/debug_data/slot1.txt");
  std::ifstream ifs2("/home/lemon/mysql/debug_data/slot1.txt");
  int a = 0, b = 0;
  int i = 0;
  while (ifs1 >> a && ifs2 >> b) {
    i++;
    if (a != b) {
      std::cout << i << std::endl;
      break;
    }
  }
}
int main() {
  ApplySystem applySystem(true);
  while (1) {
    applySystem.PopulateHashMap();
    applySystem.ApplyHashLogs();
  }
//CompareLog();
}