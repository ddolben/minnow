#ifndef DGB_WINDOW_H_
#define DGB_WINDOW_H_

#include <functional>
#include <mutex>
#include <string>

#include <SDL2/SDL.h>


namespace dgb {

// Class representing the SDL window resource. The window is created when the
// object is constructed, and corresponding state is cleaned up when the object
// is destroyed.
//
// NOTE: You MUST use this class on the main thread, since Cocoa has some weird
// intricacies about which thread is the UI thread.
class Window {
 public:
  Window(int width, int height, int texture_width, int texture_height,
         const std::string &title);

  ~Window();

  void SetRenderFunc(std::function<void()> f) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    render_func_ = f;
  }

  std::unique_lock<std::recursive_mutex> LockPixels() {
    return std::unique_lock<std::recursive_mutex>(mutex_);
  }

  // Sets the color of a pixel in the window's texture.
  // color is in 8-bit ARGB format
  void SetPixel(int x, int y, uint32_t color);

  bool IsRunning() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return running_;
  }

  void Kill() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    running_ = false;
  }

  uint8_t ScrollX() { return scroll_x_; }
  uint8_t ScrollY() { return scroll_y_; }
  void SetScrollX(uint8_t value) { scroll_x_ = value; }
  void SetScrollY(uint8_t value) { scroll_y_ = value; }

  uint8_t WindowX() { return window_x_; }
  uint8_t WindowY() { return window_y_; }
  void SetWindowX(uint8_t value) { window_x_ = value; }
  void SetWindowY(uint8_t value) { window_y_ = value; }

  // Runs a single iteration of the window's render loop.
  void Tick();

 private:
  bool running_ = true;
  int width_ = 0;
  int height_ = 0;
  int texture_width_ = 0;
  int texture_height_ = 0;
  std::string title_;

  SDL_Rect rect_;

  uint8_t scroll_x_ = 0;
  uint8_t scroll_y_ = 0;

  uint8_t window_x_ = 0;
  uint8_t window_y_ = 0;

  std::recursive_mutex mutex_;
  std::function<void()> render_func_;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  SDL_Texture *texture_ = nullptr;
  Uint32 *pixels_ = nullptr;
};

}  // namespace dgb

#endif  // DGB_WINDOW_H_
