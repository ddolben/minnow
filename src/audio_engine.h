#include <memory>
#include <vector>

#include <SDL2/SDL.h>


// DynamicRingBuffer is a ring buffer that can be resized, and dynamically
// grows the underlying memory accordingly.
template <typename T>
class DynamicRingBuffer {
 public:
  DynamicRingBuffer(T default_value = T()) : default_value_(default_value) {}
  DynamicRingBuffer(unsigned long size, T default_value = T())
      : default_value_(default_value) {
    Resize(size);
  }

  void Resize(unsigned long size) {
    data_.resize(size, default_value_);
  }

  unsigned long Size() { return data_.size(); }

  const T &operator[](unsigned long i) const {
    return data_[ i % Size() ];
  }
  T &operator[](unsigned long i) {
    return data_[ i % Size() ];
  }

 private:
  T default_value_;
  std::vector<T> data_;
};


namespace dgb {

static const int kVolumeMax = INT16_MAX;
static const int kVolumeMin = 0;
static const int kVolumeSweepStep = kVolumeMax / 16;
static const int kFrequencyMax = 20000;
static const int kFrequencyMin = 20;

static const int kFrequencyCheckRate = 128;
static const int kVolumeCheckRate = 64;
static const int kLengthCheckRate = 256;

class AudioTrack {
 public:
  AudioTrack(int sample_rate) : sample_rate_(sample_rate) {}

  // Sets the track length in seconds.
  void SetLength(float length);

  // Sets the square wave parameters.
  // 0.0 <= duty <= 1.0
  void SetSquare(int frequency, float duty);

  // TODO: description
  // sweep_step: the amout to sweep the frequency for each interval, in Hz.
  //   If sweep step is negative, frequency will sweep down.
  // sweep_interval: number of seconds per frequency sweep.
  //   A value of zero will turn off frequency sweep.
  // TODO: figure out why it craps out above ~ 6000Hz
  void SetFrequencySweep(int sweep_step, float sweep_interval);

  // Sets the volume of the track and disables volume sweep.
  // 0.0 <= volume <= 1.0
  void SetVolume(float volume);

  // Sets the initial volume and volume sweep rate of the track.
  // 0.0 <= volume <= 1.0
  // sweep_interval: number of seconds per volume sweep
  //   If sweep interval is negative, volume will sweep down.
  //   A value of zero will turn off volume sweep.
  void SetVolumeAndSweep(float volume, float sweep_interval);

  // Copies an arbitrary waveform into the sample buffer, maintaining the
  // current sample buffer's size. Performs left-nearest-neighbor interpolation
  // on the samples.
  void SetWaveform(int16_t *data, int length);

  int16_t Sample(float t);

  void DoVolumeCheck();
  void DoFrequencyCheck();
  void DoLengthCheck();

 private:
  int16_t SampleSine(float t);

  // Updates the sample buffer with a square wave.
  void UpdateBufferSquare();

  // Samples a square wave at time 't'.
  // TODO: band-limited synthesis
  int16_t SampleSquare(float t);

  // Returns the correct portion of the internal sample buffer for time 't'.
  int16_t SampleBuffer(float t);

  // Contains one full cycle's worth of samples.
  DynamicRingBuffer<int16_t> sample_buffer_;

  // Number of length checks left until the track is silenced.
  uint64_t track_length_counter_ = UINT64_MAX;
  // Samples per second for the current device.
  int sample_rate_ = 0;

  // Current frequency of the output tone, in Hz.
  unsigned frequency_ = 0;
  // Number of frequency checks until next frequency sweep step.
  unsigned frequency_sweep_counter_ = 0;
  // Value to copy into frequency_sweep_counter_ when it hits zero.
  unsigned frequency_sweep_interval_ = 0;
  // Amount to change frequency for each step, in Hz.
  // TODO: description
  int frequency_sweep_step_ = 0;

  // Number of samples for positive half of a square wave duty.
  int square_duty_ = 0;
  float square_duty_fraction_ = 0.0f;

  int volume_ = 0;  // 0 - int16 max (0x7FFF for now)
  // Number of volume checks until next volume sweep step.
  unsigned volume_sweep_counter_ = 0;
  // Value to copy into volume_sweep_counter_ when it hits zero.
  unsigned volume_sweep_interval_ = 0;
  int volume_sweep_direction_ = 1;  // -1 or 1
  bool do_volume_sweep_ = false;
};

class AudioEngine {
 public:
  void AddTrack(std::shared_ptr<AudioTrack> track) {
    tracks_.push_back(track);
  }

  int16_t Sample(float t);

  static void AudioCallback(void *user_data, Uint8* stream, int length);

  // Initializes the SDL audio device.
  // Make sure this object is not destroyed before the audio device is shut
  // down.
  // TODO: destroy the device when this object is destroyed
  void Init();

  int sample_rate() { return sample_rate_; }

 private:
  // Decrement all the check counters and execute any necessary checks.
  void DoChecks();

  // Check to see if any modifications need to be made to any tracks.
  void DoVolumeChecks();
  void DoFrequencyChecks();
  void DoLengthChecks();

  std::vector<std::shared_ptr<AudioTrack>> tracks_;

  uint64_t audio_pos_ = 0;
  int sample_rate_ = 0;
  float seconds_per_sample_ = 0;

  // Counters to synchronize volume, frequency, and length checks across tracks.
  unsigned volume_check_interval_ = 0;
  unsigned volume_check_counter_ = 0;
  unsigned frequency_check_interval_ = 0;
  unsigned frequency_check_counter_ = 0;
  unsigned length_check_interval_ = 0;
  unsigned length_check_counter_ = 0;

  SDL_AudioDeviceID device_;
};

}  // namespace dgb
