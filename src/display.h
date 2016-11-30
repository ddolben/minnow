#ifndef DGB_DISPLAY_H_
#define DGB_DISPLAY_H_

#include <functional>
#include <memory>
#include <mutex>

#include <SDL2/SDL.h>

#include "clock.h"
#include "logging.h"


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
    CHECK(0 <= x && x < texture_width_ && 0 <= y && y < texture_height_);
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

// Used to apply structure on top of tile memory.
struct Tile {
  uint8_t data[16];
};

// Class representing the display device.
class Display {
 public:
  const static uint16_t kVRAMSize = 8192;
  const static uint16_t kSpriteAttributeTableSize = 0xA0;
  const static int kCycleLength = 70224;
  const static int kLineCount = 144;
  const static int kLineCycleCount = 456;
  const static int kVBlankCycleCount = 4560;

  Display(int width, int height, std::shared_ptr<Clock> clock) {
    window_controller_.reset(new WindowController());
    window_.reset(new Window(width, height, 256, 256));
    window_controller_->AddWindow(window_);
    tileset_window_.reset(new Window(256, 192, 128, 96));
    window_controller_->AddWindow(tileset_window_);

    clock->RegisterObserver([this](int cycles) {
      // TODO: implement interrupts
      this->AdvanceClock(cycles);
    });
  }

  void AdvanceClock(int cycles) {
    cycle_clock_ = (cycle_clock_ + cycles) % kCycleLength;
  }

  uint8_t LCDCY() {
    return cycle_clock_ / kLineCycleCount;
  }

  uint8_t Control() { return control_; }
  void SetControl(uint8_t value) {
    control_ = value;
  }

  uint8_t Status() { return status_; }
  void SetStatus(uint8_t value) {
    // TODO: enforce read-only bits
    status_ = value;
  }

  uint32_t Color(uint8_t value) {
    int shift = value*2;
    uint8_t idx = (palette_ >> shift) & 0x3;
    switch (idx) {
    case 0: return 0xffffffff;
    case 1: return 0xaaaaaaaa;
    case 2: return 0x44444444;
    }
    return 0;
  }

  void SetPalette(uint8_t value) {
    palette_ = value;
  }

  void SetObjectPalette0(uint8_t value) { object_palette_0_ = value; }
  void SetObjectPalette1(uint8_t value) { object_palette_1_ = value; }

  bool IsRunning() { return window_->IsRunning(); }

  uint8_t ScrollX() { return window_->ScrollX(); }
  uint8_t ScrollY() { return window_->ScrollY(); }
  void SetScrollX(uint8_t value) { window_->SetScrollX(value); }
  void SetScrollY(uint8_t value) { window_->SetScrollY(value); }

  uint8_t WindowX() { return window_->WindowX(); }
  uint8_t WindowY() { return window_->WindowY(); }
  void SetWindowX(uint8_t value) { window_->SetWindowX(value); }
  void SetWindowY(uint8_t value) { window_->SetWindowY(value); }

  // Returns one of the tiles in the 32 x 32 tileset, indexed by x and y
  // coordinates. Looks into the tilemap and returns the corresponding tile.
  Tile *GetTile(int x, int y) {
    CHECK(0 <= x && x <= 32);
    CHECK(0 <= y && y <= 32);
    // TODO: this mutex lock doesn't actually protect against accessing the
    // tile's memory after this function returns.
    std::lock_guard<std::mutex> lock(mutex_);
    uint8_t tile_id = tilemap_[y*32 + x];
    return reinterpret_cast<struct Tile*>(vram_ + (tile_id * 16));
  }

  uint8_t Read8(uint16_t offset) {
    CHECK(0 <= offset && offset <= kVRAMSize - 1);
    std::lock_guard<std::mutex> lock(mutex_);
    return vram_[offset];
  }

  uint16_t Read16(uint16_t offset) {
    CHECK(0 <= offset && offset <= kVRAMSize - 2);
    std::lock_guard<std::mutex> lock(mutex_);
    return *reinterpret_cast<uint16_t*>(vram_ + offset);
  }

  void Write8(uint16_t offset, uint8_t value) {
    CHECK(0 <= offset && offset <= kVRAMSize - 1);
    std::lock_guard<std::mutex> lock(mutex_);
    vram_[offset] = value;
  }

  void WriteSprite8(uint16_t offset, uint8_t value) {
    sprite_attributes_[offset] = value;
  }

  void Write16(uint16_t offset, uint16_t value) {
    CHECK(0 <= offset && offset <= kVRAMSize - 2);
    std::lock_guard<std::mutex> lock(mutex_);
    *reinterpret_cast<uint16_t*>(vram_ + offset) = value;
  }

  void Loop() {
    // Draw the tilemap into the main window.
    window_->SetRenderFunc([this]{
      for (int ty = 0; ty < 32; ++ty) {
        for (int tx = 0; tx < 32; ++tx) {
          Tile *tile = this->GetTile(tx, ty);

          for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
              // TODO: this belongs in a function
              uint8_t *b = tile->data + (y*2);
              uint8_t value = ((*b >> (7-x)) & 0x1) | (((*(b+1) >> (7-x)) & 0x1) << 1);
              this->window_->SetPixel(
                  (tx*8 + x),
                  (ty*8 + y),
                  this->Color(value));
            }
          }
        }
      }
    });

    // Draw the tileset out to the second window.
    tileset_window_->SetRenderFunc([this]{
      int tx, ty;
      for (int t = 0; t < 192; ++t) {
        tx = t % 16;
        ty = t / 16;
        Tile *tile = reinterpret_cast<Tile*>(this->vram_ + (t * 16));

        for (int y = 0; y < 8; ++y) {
          for (int x = 0; x < 8; ++x) {
            uint8_t *b = tile->data + (y*2);
            uint8_t value = ((*b >> (7-x)) & 0x1) | (((*(b+1) >> (7-x)) & 0x1) << 1);
            this->tileset_window_->SetPixel(
                (tx*8 + x),
                (ty*8 + y),
                this->Color(value));
          }
        }
      }
    });

    // Loop.
    while (window_controller_->Tick()) {}
  }

 private:
  uint8_t control_ = 0;
  uint8_t status_ = 0;

  uint8_t palette_ = 0;
  uint8_t object_palette_0_ = 0;
  uint8_t object_palette_1_ = 0;

  std::mutex mutex_;
  uint8_t vram_[kVRAMSize];
  uint8_t *tilemap_ = vram_ + 0x1800;
  uint8_t sprite_attributes_[kSpriteAttributeTableSize];

  // Used to figure out where the display is in its cycle.
  int cycle_clock_ = 0;

  std::unique_ptr<WindowController> window_controller_;
  std::shared_ptr<Window> window_;
  std::shared_ptr<Window> tileset_window_;
};

}  // namespace dgb

#endif  // DGB_DISPLAY_H_
