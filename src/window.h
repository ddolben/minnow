#ifndef DGB_WINDOW_H_
#define DGB_WINDOW_H_

#include <functional>
#include <memory>
#include <mutex>

#include <SDL2/SDL.h>

namespace dgb {

// Class representing the SDL window resource. The window is created when the
// object is constructed, and corresponding state is cleaned up when the object
// is destroyed.
//
// The methods of this class are threadsafe EXCEPT StartLoop().
//
// NOTE: You MUST use this class on the main thread, since Cocoa has some weird
// intricacies about which thread is the UI thread.
class Window {
 public:
  static const int kDisplayWidth = 160;
  static const int kDisplayHeight = 144;

  Window(int width, int height, int texture_width, int texture_height)
      : width_(width), height_(height), texture_width_(texture_width),
        texture_height_(texture_height) {
    // TODO: do this only once?
    //SDL_Init(SDL_INIT_VIDEO);

    window_ = SDL_CreateWindow("DGB Emulator",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width_, height_, 0);
    renderer_ = SDL_CreateRenderer(window_, -1, 0);
    texture_ = SDL_CreateTexture(renderer_,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
        texture_width_, texture_height_);
    pixels_ = new Uint32[texture_width_ * texture_height_];
    memset(pixels_, 0, texture_width_ * texture_height_ * sizeof(Uint32));
  }

  ~Window() {
    delete[] pixels_;

    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
  }

  void SetRenderFunc(std::function<void()> f) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    render_func_ = f;
  }

  // Sets the color of a pixel in the window's texture.
  // color is in 8-bit ARGB format
  void SetPixel(int x, int y, uint32_t color) {
    //CHECK(0 <= x && x < texture_width_ && 0 <= y && y < texture_height_);
    if (!(0 <= x && x < texture_width_ && 0 <= y && y < texture_height_)) return;
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    pixels_[(y * texture_width_) + x] = color;
  }

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

  void Tick() {
    if (!IsRunning()) {
      return;
    }

    rect_.x = 0;
    rect_.y = 0;
    // Stretch the texture to double its size.
    rect_.w = width_;
    rect_.h = height_;

    // Copy the display's pixels into the framebuffer. Make sure they aren't
    // edited while we're doing this.
    {
      std::lock_guard<std::recursive_mutex> lock(mutex_);
      if (render_func_ != nullptr) {
        render_func_();
      }
      SDL_UpdateTexture(texture_, NULL, pixels_, texture_width_ * sizeof(Uint32));
    }

    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, NULL, &rect_);

    int frame_x = scroll_x_ * 2;
    int frame_y = scroll_y_ * 2;
    SDL_Rect rectToDraw = {
      frame_x,
      frame_y,
      kDisplayWidth*2,
      kDisplayHeight*2};
    SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
    SDL_RenderDrawRect(renderer_, &rectToDraw);

    SDL_RenderPresent(renderer_);
  }

 private:
  bool running_ = true;
  int width_ = 0;
  int height_ = 0;
  int texture_width_ = 0;
  int texture_height_ = 0;

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

// Provides the looping mechanism used to control one or more Windows.
//
// This class is NOT thread-safe.
//
// NOTE: currently, I think you must create this class first. It should be
// destroyed after all of its members, due to the shared_ptr's.
class WindowController {
 public:
  WindowController() {
    // TODO: do this only once?
    SDL_Init(SDL_INIT_VIDEO);
  }

  ~WindowController() {
    SDL_Quit();
  }

  void AddWindow(std::shared_ptr<Window> window) {
    windows_.push_back(window);
  }

  bool Tick() {
    if (!running_) return false;

    while (SDL_PollEvent(&event_)) {
      // TODO: when stuck in a loop, several events appears to trigger some
      // invalid instructions (specifically, 0xbfff <- 0x39)
      switch (event_.type) {
        case SDL_QUIT:
          running_ = false;
          break;
      }
    }

    if (!running_) return false;

    for (std::shared_ptr<Window> &window : windows_) {
      window->Tick();
    }

    return true;
  }

 private:
  bool running_ = true;
  std::vector<std::shared_ptr<Window>> windows_;
  SDL_Event event_;
};

}  // namespace dgb

#endif  // DGB_WINDOW_H_
