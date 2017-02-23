#include "sound_controller.h"

#include "common/logging.h"

namespace dgb {

SoundController::SoundController() {
  audio_engine_.Init();

  track1_.reset(new AudioTrack(audio_engine_.sample_rate()));
  track1_->SetVolume(0);
  track1_->SetLength(0);
  audio_engine_.AddTrack(track1_);

  track2_.reset(new AudioTrack(audio_engine_.sample_rate()));
  track2_->SetVolume(0);
  track2_->SetLength(0);
  audio_engine_.AddTrack(track2_);
}

void SoundController::UpdateChannel1() {
  if ((channel1_frequency_high_ & 0x40) == 0) {
    // TODO: infinite length
    track1_->SetLength(1000.0);
  } else {
    float length = static_cast<float>(64 - (channel1_pattern_ & 0x3F)) / 256;
    track1_->SetLength(length);
  }

  int frequency_x = 
    channel1_frequency_low_ + ((channel1_frequency_high_ & 0x7) * 0x100);
  int frequency = 131072 / (2048 - frequency_x);
  if (frequency > 3000) {
    //printf("%d\n", frequency);
    frequency = 0;
  }

  float duty = 0;
  switch ((channel1_pattern_ & 0xC0) >> 6) {
    case 0x0: duty = 0.125f; break;
    case 0x1: duty = 0.25f; break;
    case 0x2: duty = 0.5f; break;
    default: duty = 0.75f; break;
  }
  track1_->SetSquare(frequency, duty);

  // TODO: make this a multiply
  float volume = static_cast<float>((channel1_volume_ & 0xF0) >> 4) / 16.0;
  float envelope_direction = ((channel1_volume_ & 0x8) == 0) ? -1.0 : 1.0;
  int envelope_number = (channel1_volume_ & 0x7);
  float sweep_interval = envelope_direction *
      static_cast<float>(envelope_number) / 64.0;
  if (envelope_number == 0) {
    track1_->SetVolume(volume);
  } else {
    track1_->SetVolumeAndSweep(volume, sweep_interval);
  }
}

void SoundController::UpdateChannel2() {
  if ((channel2_frequency_high_ & 0x40) == 0) {
    // TODO: infinite length
    track2_->SetLength(1000.0);
  } else {
    float length = static_cast<float>(64 - (channel2_pattern_ & 0x3F)) / 256;
    track2_->SetLength(length);
  }

  int frequency_x = 
    channel2_frequency_low_ + ((channel2_frequency_high_ & 0x7) * 0x100);
  int frequency = 131072 / (2048 - frequency_x);
  if (frequency > 3000) {
    //printf("%d\n", frequency);
    frequency = 0;
  }

  float duty = 0;
  switch ((channel2_pattern_ & 0xC0) >> 6) {
    case 0x0: duty = 0.125f; break;
    case 0x1: duty = 0.25f; break;
    case 0x2: duty = 0.5f; break;
    default: duty = 0.75f; break;
  }
  track2_->SetSquare(frequency, duty);

  // TODO: make this a multiply
  float volume = static_cast<float>((channel2_volume_ & 0xF0) >> 4) / 16.0;
  float envelope_direction = ((channel2_volume_ & 0x8) == 0) ? -1.0 : 1.0;
  int envelope_number = (channel2_volume_ & 0x7);
  float sweep_interval = envelope_direction *
      static_cast<float>(envelope_number) / 64.0;
  if (envelope_number == 0) {
    track2_->SetVolume(volume);
  } else {
    track2_->SetVolumeAndSweep(volume, sweep_interval);
  }
}

void SoundController::Write8(uint16_t address, uint8_t value) {
  if (address == 0xff10) {
    //printf("Sweep\n");
    channel1_sweep_ = value;
    float interval;
		switch ((value & 0x70) >> 4) {
			case 0x1: interval = 0.0078; break;  // (1/128Hz)
			case 0x2: interval = 0.0156; break;  // (2/128Hz)
			case 0x3: interval = 0.0234; break;  // (3/128Hz)
			case 0x4: interval = 0.0313; break;  // (4/128Hz)
			case 0x5: interval = 0.0391; break;  // (5/128Hz)
			case 0x6: interval = 0.0469; break;  // (6/128Hz)
			case 0x7: interval = 0.0547; break;  // (7/128Hz)
			default: interval = 0;
		}
    float sweep_direction = ((value & 4) == 0) ? 1.0 : -1.0;
		int sweep_number = (value & 0x7);
    track1_->SetFrequencySweep(sweep_direction * interval, sweep_number);
    return;
  }
  if (address == 0xff11) {
    channel1_pattern_ = value;
    return;
  }
  if (address == 0xff12) {
    channel1_volume_ = value;
    // Note that this does NOT reset channel 1. The channel will pick up volume
    // changes on the next reset.
    return;
  }
  if (address == 0xff13) {
    channel1_frequency_low_ = value;
    return;
  }
  if (address == 0xff14) {
    channel1_frequency_high_ = value;
    if ((channel1_frequency_high_ & 0x80) != 0) {
      //printf("Restart sound 1\n");
      UpdateChannel1();
    } else {
      //printf("Don't restart sound 1\n");
    }
    return;
  }

  if (address == 0xff16) {
    channel2_pattern_ = value;
    return;
  }
  if (address == 0xff17) {
    channel2_volume_ = value;
    // Note that this does NOT reset channel 2. The channel will pick up volume
    // changes on the next reset.
    return;
  }
  if (address == 0xff18) {
    channel2_frequency_low_ = value;
    return;
  }
  if (address == 0xff19) {
    channel2_frequency_high_ = value;
    if ((channel2_frequency_high_ & 0x80) != 0) {
      //printf("Restart sound 2\n");
      UpdateChannel2();
    } else {
      //printf("Don't restart sound 2\n");
    }
    return;
  }

  if (address == 0xff1a) { channel3_on_off_ = value; return; }
  if (address == 0xff1b) { channel3_length_ = value; return; }
  if (address == 0xff1c) { channel3_level_ = value; return; }
  if (address == 0xff1d) { channel3_frequency_low_ = value; return; }
  if (address == 0xff1e) { channel3_frequency_high_ = value; return; }

  if (address == 0xff20) { channel4_length_ = value; return; }
  if (address == 0xff21) { channel4_volume_ = value; return; }
  if (address == 0xff22) { channel4_polynomial_ = value; return; }
  if (address == 0xff23) { channel4_settings_ = value; return; }
  
  if (0xff30 <= address && address <= 0xff3f) {
    channel3_wave_pattern_[address - 0xff30] = value;
    return;
  }

  FATALF("TODO: write to sound controller: 0x%04x <- 0x%02x", address, value);
}

uint8_t SoundController::Read8(uint16_t address) {
  if (address == 0xff10) { return channel1_sweep_; }
  if (address == 0xff11) { return channel1_pattern_; }
  if (address == 0xff12) { return channel1_volume_; }
  if (address == 0xff13) { return channel1_frequency_low_; }
  if (address == 0xff14) { return channel1_frequency_high_; }

  if (address == 0xff16) { return channel2_pattern_; }
  if (address == 0xff17) { return channel2_volume_; }
  if (address == 0xff18) { return channel2_frequency_low_; }
  if (address == 0xff19) { return channel2_frequency_high_; }

  if (address == 0xff1a) { return channel3_on_off_; }
  if (address == 0xff1b) { return channel3_length_; }
  if (address == 0xff1c) { return channel3_level_; }
  if (address == 0xff1d) { return channel3_frequency_low_; }
  if (address == 0xff1e) { return channel3_frequency_high_; }

  if (address == 0xff20) { return channel4_length_; }
  if (address == 0xff21) { return channel4_volume_; }
  if (address == 0xff22) { return channel4_polynomial_; }
  if (address == 0xff23) { return channel4_settings_; }

  if (0xff30 <= address && address <= 0xff3f) {
    return channel3_wave_pattern_[address - 0xff30];
  }

  FATALF("TODO: read from sound controller: 0x%04x", address);
  return 0;
}

}  // namespace dgb
