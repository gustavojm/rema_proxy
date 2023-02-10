#ifndef TIMER_HPP
#define TIMER_HPP

#include <iostream>
#include <chrono>
#include <thread>
#include <functional>

class Timer {
 public:
  Timer(std::function<void()> callback, int interval)
      : callback_(callback),
        interval_(interval),
        stop_(false) {
    thread_ = std::thread([&]() {
      while (!stop_) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_));
        callback_();
      }
    });
  }

  ~Timer() {
    stop_ = true;
    thread_.join();
  }

 private:
  std::function<void()> callback_;
  int interval_;
  std::thread thread_;
  bool stop_;
};

#endif 		// TIMER_HPP
