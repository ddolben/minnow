#ifndef DGB_TIMERS_H_
#define DGB_TIMERS_H_

#include <chrono>


namespace dgb {

static const uint64_t kDividerPeriod = 16384;

class Timers {
 public:
  // Accessible at 0xFF04. Incremented at 16384Hz.
  uint8_t Divider() {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - last_reset_).count();
    uint64_t count = elapsed / kDividerPeriod;
    return count % 256;
  }

  void ResetDivider() {
    last_reset_ = std::chrono::high_resolution_clock::now();
  }

 private:
  std::chrono::high_resolution_clock::time_point last_reset_ =
      std::chrono::high_resolution_clock::now();
};

}  // namespace dgb

#endif  // DGB_TIMERS_H_
