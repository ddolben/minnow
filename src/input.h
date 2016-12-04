#ifndef DGB_INPUT_H_
#define DGB_INPUT_H_

#include <mutex>


namespace dgb {

enum Buttons {
  BUTTON_START = 0x80,
  BUTTON_SELECT = 0x40,
  BUTTON_B = 0x20,
  BUTTON_A = 0x10,
  BUTTON_DOWN = 0x08,
  BUTTON_UP = 0x04,
  BUTTON_LEFT = 0x02,
  BUTTON_RIGHT = 0x01
};

class Input {
 public:
  uint8_t Joypad() {
    std::lock_guard<std::mutex> lock(mutex_);
    return joypad_;
  }

  void SetJoypad(uint8_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Only set the bits 4-7 (0-3 are read-only).
    joypad_ = (value & 0xf0) | (joypad_ & 0xf);
    UpdateJoypad();
  }

  void SetButtons(uint8_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    buttons_ = ~value;
    UpdateJoypad();
  }

 private:
  void UpdateJoypad() {
    if ((joypad_ & (1 << 5)) == 0) {
      // Button Keys
      joypad_ = (joypad_ & 0xf0) | (buttons_ >> 4);
    } else if ((joypad_ & (1 << 4)) == 0) {
      // Direction Keys
      joypad_ = (joypad_ & 0xf0) | (buttons_ & 0x0f);
    }
  }

  // Bit 7 - Not used
  // Bit 6 - Not used
  // Bit 5 - P15 Select Button Keys      (0=Select)
  // Bit 4 - P14 Select Direction Keys   (0=Select)
  // Bit 3 - P13 Input Down  or Start    (0=Pressed) (Read Only)
  // Bit 2 - P12 Input Up    or Select   (0=Pressed) (Read Only)
  // Bit 1 - P11 Input Left  or Button B (0=Pressed) (Read Only)
  // Bit 0 - P10 Input Right or Button A (0=Pressed) (Read Only)
  uint8_t joypad_ = 0x0f;

  // NOTE: A value of 0 means the button is pressed
  // Bit 7 - Start
  // Bit 6 - Select
  // Bit 5 - Button B
  // Bit 4 - Button A
  // Bit 3 - Down
  // Bit 2 - Up
  // Bit 1 - Left
  // Bit 0 - Right
  uint8_t buttons_ = 0;

  std::mutex mutex_;
};

}  // namespace dgb

#endif   // DGB_INPUT_H_
