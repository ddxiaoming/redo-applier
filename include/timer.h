#pragma once
#include <chrono>
#include <fmt/chrono.h>

/**
 * 计时器，用于打印某一个函数的运行时间
 * @tparam T 时间精度，可以是std::chrono::nanoseconds、std::chrono::microseconds、std::chrono::milliseconds、
 * std::chrono::seconds、std::chrono::minutes、std::chrono::hours
 */
class Timer {
public:
  Timer() : start_time_(std::chrono::steady_clock::now()) { }
  ~Timer() {
    auto end_time = std::chrono::steady_clock::now();
    parse_body_time += (end_time - start_time_).count();
  }
private:
  std::chrono::time_point<std::chrono::steady_clock> start_time_;
};