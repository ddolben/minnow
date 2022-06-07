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

  uint8_t get_counter() { return counter_; }
  void set_counter(uint8_t value) { counter_ = value; }

  uint8_t control() { return control_; }
  void set_control(uint8_t value) {
    control_ = value;
    switch (control_ & 0x3) {
      case 0x3: resolution_ = kClockSpeed / 16384; break;
      case 0x2: resolution_ = kClockSpeed / 65536; break;
      case 0x1: resolution_ = kClockSpeed / 262144; break;
      default: resolution_ = kClockSpeed / 4096; break;
    }
  }

 private:
  const uint64_t kClockSpeed = 4194304;

  void AdvanceClock(int cycles) {
    if ((control_ & TIMER_ENABLE_BIT) == 0) return;

    cycles_accumulator_ += cycles;
    if (cycles_accumulator_ > resolution_) {
      if (counter_ == 0xFF) {
        counter_ = modulo_;
        interrupts_->SignalInterrupt(INTERRUPT_TIMER);
      } else {
        counter_++;
      }
      cycles_accumulator_ -= resolution_;
    }
  }

  uint8_t modulo_ = 0;  // 0xFF06
  uint8_t control_ = 0;  // 0xFF07

  uint8_t counter_ = 0;
  uint64_t resolution_ = 1;
  uint64_t cycles_accumulator_ = 0;

  std::chrono::high_resolution_clock::time_point last_reset_ =
      std::chrono::high_resolution_clock::now();

  std::shared_ptr<Clock> clock_;
  std::shared_ptr<Interrupts> interrupts_;
};

}  // namespace dgb

#endif  // DGB_TIMERS_H_
