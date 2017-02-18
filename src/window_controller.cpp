#include "window_controller.h"

namespace dgb {

bool WindowController::Tick() {
  // TODO: interrupts
  while (SDL_PollEvent(&event_)) {
    // TODO: when stuck in a loop, several events appears to trigger some
    // invalid instructions (specifically, 0xbfff <- 0x39)
    switch (event_.type) {
      case SDL_KEYUP:
        if (event_.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
          dispatch_->FireEvent(Event(EVENT_START_DEBUGGER));
        }
        if (event_.key.keysym.scancode == SDL_SCANCODE_SPACE) {
          dispatch_->FireEvent(Event(EVENT_TOGGLE_PAUSE));
        }
        if (event_.key.keysym.scancode == SDL_SCANCODE_A) {
          Event e = Event(EVENT_THROTTLE);
          e.set_bool_value(true);
          dispatch_->FireEvent(e);
        }
        break;
      case SDL_KEYDOWN:
        if (event_.key.keysym.scancode == SDL_SCANCODE_A) {
          Event e = Event(EVENT_THROTTLE);
          e.set_bool_value(false);
          dispatch_->FireEvent(e);
        }
        break;
      case SDL_QUIT:
        running_ = false;
        break;
    }
  }

  CheckInput();

  if (!running_) return false;

  // Wait for the CPU to signal a frame draw.
  // Include a timeout so we don't get stuck here if the CPU is paused.
  framerate_sync_.WaitWithTimeout(100);

  for (std::shared_ptr<Window> &window : windows_) {
    window->Tick();
  }

  if (print_fps_) {
    DoFPS();
  }

  return true;
}

void WindowController::DoFPS() {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      now - last_fps_time_);
  const static std::chrono::nanoseconds kFPSInterval{1000000000};
  if (elapsed > kFPSInterval) {
    printf("%d fps\n", frame_counter);
    frame_counter = 0;
    last_fps_time_ = now;
  } else {
    ++frame_counter;
  }
}

void WindowController::CheckInput() {
  // Check keyboard state and update input.
  const Uint8 *state = SDL_GetKeyboardState(NULL);
  uint8_t buttons = 0;
  if (state[SDL_SCANCODE_DOWN]) buttons |= BUTTON_DOWN;
  if (state[SDL_SCANCODE_UP]) buttons |= BUTTON_UP;
  if (state[SDL_SCANCODE_LEFT]) buttons |= BUTTON_LEFT;
  if (state[SDL_SCANCODE_RIGHT]) buttons |= BUTTON_RIGHT;
  if (state[SDL_SCANCODE_RETURN]) buttons |= BUTTON_START;
  if (state[SDL_SCANCODE_TAB]) buttons |= BUTTON_SELECT;
  if (state[SDL_SCANCODE_X]) buttons |= BUTTON_A;
  if (state[SDL_SCANCODE_Z]) buttons |= BUTTON_B;

  input_->SetButtons(buttons);
}

}  // namespace dgb
