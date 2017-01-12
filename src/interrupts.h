#ifndef DGB_INTERRUPTS_H_
#define DGB_INTERRUPTS_H_

namespace dgb {

enum InterruptType {
  INTERRUPT_VBLANK = 0x1,
  INTERRUPT_LCD_STAT = 0x2,
  INTERRUPT_TIMER = 0x4,
  INTERRUPT_SERIAL = 0x8,
  INTERRUPT_JOYPAD = 0x10
};

class Interrupts {
 public:
  void RegisterInterruptHandler(std::function<void(uint8_t)> f) {
    handler_ = f;
  }

  void SignalInterrupt(uint8_t type) {
    printf("Interrupt: 0x%02x\n", type);
    if (handler_ != nullptr) {
      handler_(type);
    }
  }

 private:
  std::function<void(uint8_t)> handler_;
};

}  // namespace dgb

#endif  // DGB_INTERRUPTS_H_
