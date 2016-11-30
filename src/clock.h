#ifndef DGB_CLOCK_H_
#define DGB_CLOCK_H_

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include <iostream>


namespace dgb {

class Clock {
 public:
  // CPU clock speed: 4.194304 MHz (0.2384 us per cycle)
  const std::chrono::nanoseconds kCPUCycleTime{238};  // Actually 2384
  const uint64_t kSyncCycles = 4194;  // Roughly 1ms worth of cycles

  void Tick(int cycles) {
    for (std::function<void(int)> f : callbacks_) {
      f(cycles);
    }

    // If we executed faster than the GameBoy CPU would have, wait for a bit to
    // sync up. Do this every 1ms of expected CPU time to mask any inaccuracies
    // in the sleep function due to interrupts, threading, or other system
    // activity.
    cycle_accumulator_ += cycles;
    if (cycle_accumulator_ > kSyncCycles) {
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
          now - last_sync_time_);
      std::chrono::nanoseconds expected_elapsed =
          kCPUCycleTime * cycle_accumulator_;
      if (elapsed < expected_elapsed) {
        std::this_thread::sleep_for(expected_elapsed - elapsed);
        //std::this_thread::sleep_for(kCPUCycleTime);
      }
      last_sync_time_ = std::chrono::high_resolution_clock::now();
      cycle_accumulator_ = 0;
    }
  }

  void RegisterObserver(std::function<void(int)> f) {
    callbacks_.push_back(f);
  }

 private:
  std::vector<std::function<void(int)>> callbacks_;
  std::chrono::high_resolution_clock::time_point last_sync_time_ =
      std::chrono::high_resolution_clock::now();
  uint64_t cycle_accumulator_ = 0;
};

}  // namespace dgb

#endif  // DGB_CLOCK_H_
