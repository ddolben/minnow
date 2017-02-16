#ifndef MINNOW_SOUND_CONTROLLER_H_
#define MINNOW_SOUND_CONTROLLER_H_

#include <cstdint>


namespace dgb {

class SoundController {
 public:
  void Write8(uint16_t address, uint8_t value);
  uint8_t Read8(uint16_t address);

  uint8_t channel_control() { return channel_control_; }
  void set_channel_control(uint8_t value) { channel_control_ = value; }

  uint8_t output_select() { return output_select_; }
  void set_output_select(uint8_t value) { output_select_ = value; }

 private:
  uint8_t channel_control_ = 0;
  uint8_t output_select_ = 0;

  uint8_t channel1_pattern_ = 0;
  uint8_t channel2_pattern_ = 0;
};

}  // namespace dgb

#endif  // MINNOW_SOUND_CONTROLLER_H_
