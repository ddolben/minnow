#include "audio_engine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <SDL2/SDL.h>

#include "common/logging.h"


namespace dgb {

void AudioTrack::SetLength(float length) {
  track_length_counter_ = length * kLengthCheckRate;
}

// Samples a square wave.
// t:  between 0.0 and 1.0 and represents the time within a single cycle.
// duty:  between 0.0 and 1.0
static int16_t SampleSquare(float t, float duty) {
  return (t >= duty) ? 1 : -1;
}

void AudioTrack::SetSquare(int frequency, float duty) {
  // TODO: bounds check
  frequency_ = frequency;
  // TODO: bounds check
  if (frequency <= 0) {
    square_duty_ = 0;
  } else {
    square_duty_ = (sample_rate_ / frequency_) * duty;
  }
  square_duty_fraction_ = duty;

  UpdateBufferSquare();
}

void AudioTrack::SetFrequencySweep(int sweep_step, float sweep_interval) {
  frequency_sweep_step_ = sweep_step;
  frequency_sweep_interval_ = kFrequencyCheckRate * std::abs(sweep_interval);
  frequency_sweep_counter_ = frequency_sweep_interval_;
}

void AudioTrack::SetVolume(float volume) {
  volume_ = volume * kVolumeMax;
  do_volume_sweep_ = false;
}

void AudioTrack::SetVolumeAndSweep(float volume, float sweep_interval) {
  // TODO: bounds check
  volume_ = volume * kVolumeMax;
  volume_sweep_direction_ = (sweep_interval >= 0.0) ? 1 : -1;
  volume_sweep_interval_ = kVolumeCheckRate * std::abs(sweep_interval);
  volume_sweep_counter_ = volume_sweep_interval_;
  do_volume_sweep_ = true;
}

void AudioTrack::SetWaveform(int16_t *data, int length) {
  CHECK(length <= sample_buffer_.Size());
  unsigned length_ratio = sample_buffer_.Size() / length;
  unsigned data_index;
  for (unsigned i = 0; i < sample_buffer_.Size(); ++i) {
    data_index = i / length_ratio;
    sample_buffer_[i] = data[data_index];
  }
}

int16_t AudioTrack::Sample(float t) {
  if (track_length_counter_ <= 0 || frequency_ <= 0 || volume_ <= 0) {
    return 0;
  }
  //return SampleSquare(t);
  // TODO: synchronize access to the buffer
  return SampleBuffer(t);
}

int16_t AudioTrack::SampleSine(float t) {
  return volume_ * std::sin(2.0 * M_PI * t * frequency_);
}

void AudioTrack::UpdateBufferSquare() {
  if (frequency_ <= 0) return;  // TODO: what to do here?
  unsigned cycles_in_sample = sample_rate_ / frequency_;
  sample_buffer_.Resize(cycles_in_sample);
  for (unsigned i = 0; i < cycles_in_sample; ++i) {
    sample_buffer_[i] = dgb::SampleSquare(
        static_cast<float>(i) / cycles_in_sample,
        square_duty_fraction_);
  }
}

int16_t AudioTrack::SampleSquare(float t) {
  return volume_ * ((
      static_cast<int>(t * sample_rate_) % (sample_rate_ / frequency_)
          > square_duty_) ? 1 : -1);
}

int16_t AudioTrack::SampleBuffer(float t) {
  unsigned i = static_cast<int>(t * sample_rate_) % sample_buffer_.Size();
  return volume_ * sample_buffer_[i];
}

void AudioTrack::DoVolumeCheck() {
  if (!do_volume_sweep_) return;

  --volume_sweep_counter_;
  if (volume_sweep_counter_ <= 0) {
    volume_ += kVolumeSweepStep * volume_sweep_direction_;
    if (volume_ > kVolumeMax) volume_ = kVolumeMax;
    if (volume_ < kVolumeMin) volume_ = kVolumeMin;

    volume_sweep_counter_ = volume_sweep_interval_;
    //printf("volume: %d %d %d %d\n", volume_sweep_direction_, kVolumeMax, kVolumeSweepStep, volume_);
  }
}

void AudioTrack::DoFrequencyCheck() {
  --frequency_sweep_counter_;
  if (frequency_sweep_counter_ <= 0) {
    frequency_ += frequency_ / std::pow(2.0, frequency_sweep_step_);
    square_duty_ = (sample_rate_ / frequency_) * square_duty_fraction_;
    if (frequency_ > kFrequencyMax) frequency_ = kFrequencyMax;
    if (frequency_ < kFrequencyMin) frequency_ = kFrequencyMin;

    frequency_sweep_counter_ = frequency_sweep_interval_;
    //printf("frequency: %d\n", frequency_);

    UpdateBufferSquare();
  }
}

void AudioTrack::DoLengthCheck() {
  --track_length_counter_;
  if (track_length_counter_ <= 0) {
    volume_ = 0;
  }
}


int16_t AudioEngine::Sample(float t) {
  int16_t sample = 0;
  for (std::shared_ptr<AudioTrack> &track : tracks_) {
    sample += track->Sample(t);
    // TODO: clamp / limit
  }
  return sample;
}

void AudioEngine::AudioCallback(
    void *user_data, Uint8* stream, int length) {
  AudioEngine *that = static_cast<AudioEngine*>(user_data);

  length /= 2;  // 16-bit signed values
  Sint16* buf = reinterpret_cast<Sint16*>(stream);
  float t;
  for(int i = 0; i < length; i++) { 
    t = static_cast<float>(that->audio_pos_) * that->seconds_per_sample_;
    buf[i] = that->Sample(t);

    that->DoChecks();

    ++that->audio_pos_;
  }
}

void AudioEngine::Init() {
  // TODO: do this elsewhere
  SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);

  SDL_AudioSpec want_spec, got_spec;
  SDL_memset(&want_spec, 0, sizeof(want_spec));  // or SDL_zero(want_spec)

  want_spec.freq = 44100;
  want_spec.format = AUDIO_S16;
  want_spec.channels = 1;
  //want_spec.samples = 4096;
  want_spec.samples = 128;
  want_spec.callback = AudioEngine::AudioCallback;
  want_spec.userdata = static_cast<void*>(this);

  device_ = SDL_OpenAudioDevice(
      NULL, 0, &want_spec, &got_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

  sample_rate_ = got_spec.freq;
  seconds_per_sample_ = 1.0 / got_spec.freq;

  // Set up volume sweep checks to happen at 64 Hz.
  volume_check_interval_ = got_spec.freq / kVolumeCheckRate;
  volume_check_counter_ = volume_check_interval_;
  // Set up frequency sweep checks to happen at 128 Hz.
  frequency_check_interval_ = got_spec.freq / kFrequencyCheckRate;
  frequency_check_counter_ = frequency_check_interval_;
  // Set up length checks to happen at 256 Hz.
  length_check_interval_ = got_spec.freq / kLengthCheckRate;
  length_check_counter_ = length_check_interval_;
  
  SDL_PauseAudioDevice(device_, 0);  // Unpause the device (paused by default).
}

void AudioEngine::DoChecks() {
  --volume_check_counter_;
  if (volume_check_counter_ == 0) {
    volume_check_counter_ = volume_check_interval_;
    DoVolumeChecks();
  }
  --frequency_check_counter_;
  if (frequency_check_counter_ == 0) {
    frequency_check_counter_ = frequency_check_interval_;
    DoFrequencyChecks();
  }
  --length_check_counter_;
  if (length_check_counter_ == 0) {
    length_check_counter_ = length_check_interval_;
    DoLengthChecks();
  }
}

void AudioEngine::DoVolumeChecks() {
  for (std::shared_ptr<AudioTrack> &track : tracks_) {
    track->DoVolumeCheck();
  }
}

void AudioEngine::DoFrequencyChecks() {
  for (std::shared_ptr<AudioTrack> &track : tracks_) {
    track->DoFrequencyCheck();
  }
}

void AudioEngine::DoLengthChecks() {
  for (std::shared_ptr<AudioTrack> &track : tracks_) {
    track->DoLengthCheck();
  }
}

}  // namespace dgb
