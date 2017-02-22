#include <memory>
#include <vector>

#include <SDL2/SDL.h>


namespace dgb {

static const int kVolumeMax = INT16_MAX;
static const int kVolumeMin = 0;
static const int kVolumeSweepStep = kVolumeMax / 16;
static const int kFrequencyMax = 20000;
static const int kFrequencyMin = 20;

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

  int16_t Sample(float t);

 private:
  int16_t SampleSine(float t);

  // Samples a square wave at time 't'.
  // TODO: band-limited synthesis
  int16_t SampleSquare(float t);

  void DoVolumeSweep();
  void DoFrequencySweep();

  // Used for determining when to do a volume sweep step.
  unsigned sample_count_ = 0;
  // Track length, measured in samples.
  uint64_t track_length_ = UINT64_MAX;
  // Samples per second for the current device.
  int sample_rate_ = 0;

  // Current frequency of the output tone, in Hz.
  unsigned frequency_ = 0;
  // Number of samples until next frequency sweep step.
  unsigned frequency_sweep_counter_ = 0;
  // Number of samples per frequency sweep step.
  unsigned frequency_sweep_interval_ = 0;
  // Amount to change frequency for each step, in Hz.
  // TODO: description
  int frequency_sweep_step_ = 0;

  // Number of samples for positive half of a square wave duty.
  int square_duty_ = 0;
  float square_duty_fraction_ = 0.0f;

  int volume_ = 0;  // 0 - int16 max (0x7FFF for now)
  // Number of samples until next volume sweep step.
  unsigned volume_sweep_counter_ = 0;
  // Number of samples per volume sweep step.
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
  std::vector<std::shared_ptr<AudioTrack>> tracks_;

  uint64_t audio_pos_ = 0;
  int sample_rate_ = 0;
  float seconds_per_sample_ = 0;

  SDL_AudioDeviceID device_;
};

}  // namespace dgb
