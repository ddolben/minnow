#ifndef DGB_INPUT_H_
#define DGB_INPUT_H_

namespace dgb {

class Input {
 public:
  uint8_t Joypad() { return joypad_; }
  void SetJoypad(uint8_t value) {
    // TODO: respect which bits can be written
    joypad_ = value;
  }

 private:
  uint8_t joypad_ = 0;
};

}  // namespace dgb

#endif   // DGB_INPUT_H_
