#ifndef MINNOW_WINDOW_CONTROLLER_H_
#define MINNOW_WINDOW_CONTROLLER_H_

#include <chrono>
#include <memory>
#include <vector>

#include <SDL2/SDL.h>

#include "common/sync/semaphore.h"
#include "event_dispatch.h"
#include "input.h"
#include "window.h"


namespace dgb {

// Provides the looping mechanism used to control one or more Windows.
//
// This class is NOT thread-safe.
//
// NOTE: currently, I think you must create this class first. It should be
// destroyed after all of its members, due to the shared_ptr's.
class WindowController {
 public:
  WindowController(
      std::shared_ptr<Input> input, std::shared_ptr<EventDispatch> dispatch)
    : input_(input), dispatch_(dispatch) {
    // TODO: do this only once?
    SDL_Init(SDL_INIT_VIDEO);
  }

  ~WindowController() {
    SDL_Quit();
  }

  // AddWindow adds a window to the controller.
  void AddWindow(std::shared_ptr<Window> window) { windows_.push_back(window); }

  // SignalFrame tells the WindowController that it's acceptable to draw a
  // frame.
  void SignalFrame() { framerate_sync_.Notify(); }

  // Tick runs a single iteration of the controller's render loop.
  bool Tick();

  void set_print_fps(bool value) { print_fps_ = value; }

 private:
  void DoFPS();
  void CheckInput();

  bool running_ = true;
  std::vector<std::shared_ptr<Window>> windows_;
  std::shared_ptr<Input> input_;
  std::shared_ptr<EventDispatch> dispatch_;
  SDL_Event event_;

  Semaphore framerate_sync_;

  bool print_fps_ = false;
  std::chrono::high_resolution_clock::time_point last_fps_time_ =
      std::chrono::high_resolution_clock::now();
  int frame_counter = 0;
};

}  // namespace dgb

#endif  // MINNOW_WINDOW_CONTROLLER_H_
