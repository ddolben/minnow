#ifndef DGB_TIMERS_H_
#define DGB_TIMERS_H_

#include <chrono>
#include <memory>

#include "clock.h"
#include "interrupts.h"


namespace dgb {

static const uint64_t kDividerPeriod = 16384;

enum TimerControl {
  TIMER_ENABLE_BIT = 0x4
};

class Timers {
 public:
  Timers(std::shared_ptr<Clock> clock, std::shared_ptr<Interrupts> interrupts)
      : clock_(clock), interrupts_(interrupts) {
    clock->RegisterObserver([this](int cycles) {
      this->AdvanceClock(cycles);
    });
  }

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

  void set_modulo(uint8_t value) { modulo_ = value; }
  void set_control(uint8_t value) { control_ = value; }

 private:
  int Resolution() {
    // TODO: select based on control value
    return 4096;
  }

  void AdvanceClock(int cycles) {
    if ((control_ & TIMER_ENABLE_BIT) == 0) return;

    cycles_accumulator_ += cycles;
    if (cycles_accumulator_ > Resolution()) {
      if (counter_ == 0xFF) {
        counter_ = modulo_;
        interrupts_->SignalInterrupt(INTERRUPT_TIMER);
      } else {
        counter_++;
      }
      cycles_accumulator_ -= Resolution();
    }
  }

  uint8_t modulo_ = 0;  // 0xFF06
  uint8_t control_ = 0;  // 0xFF07

  uint8_t counter_ = 0;
  uint64_t cycles_accumulator_ = 0;

  std::chrono::high_resolution_clock::time_point last_reset_ =
      std::chrono::high_resolution_clock::now();

  std::shared_ptr<Clock> clock_;
  std::shared_ptr<Interrupts> interrupts_;
};

}  // namespace dgb

#endif  // DGB_TIMERS_H_
