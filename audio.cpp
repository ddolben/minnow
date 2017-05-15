#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <vector>

#include <SDL2/SDL.h>

#include "audio_engine.h"


using dgb::AudioEngine;
using dgb::AudioTrack;

int main(int argc, const char *argv[]) {

  SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);

  AudioEngine audio_engine;
  audio_engine.Init();

  //std::shared_ptr<AudioTrack> track(new AudioTrack(audio_engine.sample_rate()));
  //track->SetSquare(330, 0.8);
  //track->SetVolume(0.5);
  //track->SetVolumeAndSweep(0.5, 0);
  //track->SetFrequencySweep(4, 0.1);
  //track->SetLength(10);
  //audio_engine.AddTrack(track);

  std::shared_ptr<AudioTrack> custom_track(new AudioTrack(audio_engine.sample_rate()));
  custom_track->SetSquare(330, 0.5);
  custom_track->SetVolume(0.5);
  custom_track->SetLength(10);
  int16_t val = INT16_MAX/5;
  int16_t nval = INT16_MIN/5;
  int16_t wave_data[] = {val, nval, nval, nval, nval, nval, nval, val};
  custom_track->SetWaveform(wave_data, 8);
  audio_engine.AddTrack(custom_track);

  //std::shared_ptr<AudioTrack> track2(new AudioTrack(audio_engine.sample_rate()));
  //track2->SetSquare(660, 0.8);
  //track2->SetVolume(0.5);
  ////track->SetVolumeAndSweep(0.5, 0);
  ////track->SetFrequencySweep(4, 0.1);
  //track2->SetLength(5);
  //audio_engine.AddTrack(track2);

  while(1) {
    SDL_Delay(500);
  }

}
