#ifndef MINNOW_SOUND_CONTROLLER_H_
#define MINNOW_SOUND_CONTROLLER_H_

#include <cstdint>
#include <memory>

#include "audio_engine.h"


namespace dgb {

class SoundController {
 public:
  SoundController();

  void Write8(uint16_t address, uint8_t value);
  uint8_t Read8(uint16_t address);

  uint8_t channel_control() { return channel_control_; }
  void set_channel_control(uint8_t value) { channel_control_ = value; }

  uint8_t output_select() { return output_select_; }
  void set_output_select(uint8_t value) { output_select_ = value; }

  uint8_t on_off() { return on_off_; }
  void set_on_off(uint8_t value) {
    // Only set bit 7.
    on_off_ = (on_off_ & 0x7F) | (value & 0x80);
  }

 private:
  void UpdateChannel1();
  void UpdateChannel2();

  uint8_t channel_control_ = 0;
  uint8_t output_select_ = 0;
  uint8_t on_off_ = 0;

  uint8_t channel1_sweep_ = 0;
  uint8_t channel1_pattern_ = 0;
  uint8_t channel1_volume_ = 0;
  uint8_t channel1_frequency_low_ = 0;
  uint8_t channel1_frequency_high_ = 0;

  uint8_t channel2_pattern_ = 0;
  uint8_t channel2_volume_ = 0;
  uint8_t channel2_frequency_low_ = 0;
  uint8_t channel2_frequency_high_ = 0;

  uint8_t channel3_on_off_ = 0;
  uint8_t channel3_length_ = 0;
  uint8_t channel3_level_ = 0;
  uint8_t channel3_frequency_low_ = 0;
  uint8_t channel3_frequency_high_ = 0;

  static const int kWavePatternSize = 0x10;
  uint8_t channel3_wave_pattern_[kWavePatternSize];

  uint8_t channel4_length_ = 0;
  uint8_t channel4_volume_ = 0;
  uint8_t channel4_polynomial_ = 0;
  uint8_t channel4_settings_ = 0;

  AudioEngine audio_engine_;
  std::shared_ptr<AudioTrack> track1_;
  std::shared_ptr<AudioTrack> track2_;
  std::shared_ptr<AudioTrack> track3_;
  std::shared_ptr<AudioTrack> track4_;
};

}  // namespace dgb

#endif  // MINNOW_SOUND_CONTROLLER_H_
