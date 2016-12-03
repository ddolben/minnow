#ifndef DGB_INPUT_H_
#define DGB_INPUT_H_

namespace dgb {

class Input {
 public:
  uint8_t Joypad() { return joypad_; }
  void SetJoypad(uint8_t value) {
    // Only set the bits 4-7 (0-3 are read-only).
    joypad_ = (value & 0xf0) | (joypad_ & 0xf);
  }

 private:
  // Bit 7 - Not used
  // Bit 6 - Not used
  // Bit 5 - P15 Select Button Keys      (0=Select)
  // Bit 4 - P14 Select Direction Keys   (0=Select)
  // Bit 3 - P13 Input Down  or Start    (0=Pressed) (Read Only)
  // Bit 2 - P12 Input Up    or Select   (0=Pressed) (Read Only)
  // Bit 1 - P11 Input Left  or Button B (0=Pressed) (Read Only)
  // Bit 0 - P10 Input Right or Button A (0=Pressed) (Read Only)
  uint8_t joypad_ = 0x0f;
};

}  // namespace dgb

#endif   // DGB_INPUT_H_
