#ifndef DGB_CLOCK_H_
#define DGB_CLOCK_H_

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include <iostream>

#include "common/time/time.h"


namespace dgb {

class Clock {
 public:
  Clock(bool throttle) : throttle_(throttle) {}

  void Tick(int cycles) {
    for (std::function<void(int)> &f : callbacks_) {
      f(cycles);
    }

    if (throttle_) Throttle(cycles);
  }

  void RegisterObserver(std::function<void(int)> f) {
    callbacks_.push_back(f);
  }

 private:
  // CPU clock speed: 4.194304 MHz (0.2384 us per cycle)
  const int64_t kCPUCycleTime = 238400;  // Measured in picoseconds
  // Don't do the spin wait for every CPU instruction.
  const uint64_t kSyncCycles = 4560;

  // Throttle forces the emulator to sleep for a short amount of time so that
  // it executes at the emulated CPU's native clock speed. Call this once per
  // Tick() with the number of cycles as the argument.
  // If we executed faster than the GameBoy CPU would have, wait for a bit to
  // sync up. Do this every 1ms of expected CPU time to mask any inaccuracies
  // in the sleep function due to interrupts, threading, or other system
  // activity.
  void Throttle(int cycles) {
    cycle_accumulator_ += cycles;
    if (cycle_accumulator_ > kSyncCycles) {
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
          now - last_sync_time_);
      std::chrono::nanoseconds expected_elapsed(
          (kCPUCycleTime * cycle_accumulator_) / 1000);
      if (elapsed < expected_elapsed) {
        auto delay = std::chrono::duration_cast<std::chrono::nanoseconds>(
            expected_elapsed - elapsed);
        SpinWait(delay.count());
      }
      last_sync_time_ = std::chrono::high_resolution_clock::now();
      cycle_accumulator_ = 0;
    }
  }

  std::vector<std::function<void(int)>> callbacks_;
  std::chrono::high_resolution_clock::time_point last_sync_time_ =
      std::chrono::high_resolution_clock::now();
  bool throttle_ = true;
  uint64_t cycle_accumulator_ = 0;
};

}  // namespace dgb

#endif  // DGB_CLOCK_H_
