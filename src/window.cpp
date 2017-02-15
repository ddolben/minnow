#include "window.h"

#include <SDL2/SDL.h>

namespace dgb {

Window::Window(int width, int height, int texture_width, int texture_height,
    const std::string &title)
    : width_(width), height_(height), texture_width_(texture_width),
      texture_height_(texture_height), title_(title) {
  // TODO: do this only once?
  //SDL_Init(SDL_INIT_VIDEO);

  window_ = SDL_CreateWindow(title_.c_str(),
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width_, height_, 0);
  renderer_ = SDL_CreateRenderer(window_, -1, 0);
  texture_ = SDL_CreateTexture(renderer_,
      SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
      texture_width_, texture_height_);
  pixels_ = new Uint32[texture_width_ * texture_height_];
  memset(pixels_, 0, texture_width_ * texture_height_ * sizeof(Uint32));
}

Window::~Window() {
  delete[] pixels_;

  SDL_DestroyTexture(texture_);
  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(window_);
}

void Window::SetPixel(int x, int y, uint32_t color) {
  //CHECK(0 <= x && x < texture_width_ && 0 <= y && y < texture_height_);
  if (!(0 <= x && x < texture_width_ && 0 <= y && y < texture_height_)) return;
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  pixels_[(y * texture_width_) + x] = color;
}

void Window::Tick() {
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

  SDL_RenderPresent(renderer_);
}

}  // namespace dgb
