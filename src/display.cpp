#include "display.h"

#include <cstdint>

namespace dgb {

Display::Display(int width, int height, std::shared_ptr<Clock> clock,
    std::shared_ptr<Interrupts> interrupts,
    std::shared_ptr<WindowController> window_controller)
    : interrupts_(interrupts), window_controller_(window_controller) {

  tileset_window_.reset(new Window(256, 384, 128, 192, "Minnow Tileset"));
  window_controller_->AddWindow(tileset_window_);

  background_window_.reset(new Window(
        512, 512, 256, 256, "Minnow Background"));
  window_controller_->AddWindow(background_window_);

  window_.reset(new Window(
        width, height, kDisplayWidth, kDisplayHeight, "Minnow Emulator"));
  window_controller_->AddWindow(window_);

  clock->RegisterObserver([this](int cycles) {
    this->AdvanceClock(cycles);
  });
}

void Display::AdvanceClock(int cycles) {
  cycle_clock_ = (cycle_clock_ + cycles) % kCycleLength;
  // Zero the mode bits in the LCD status register.
  status_ &= ~0x03;
  // Update the mode bits in the LCD status register.
  if (cycle_clock_ >= kVBlankStart) {
    status_ |= MODE_VBLANK_BITS;
  } else if ((cycle_clock_ % kLineCycleCount) >= 356) {
    status_ |= MODE_HBLANK_BITS;
  } else if ((cycle_clock_ % kLineCycleCount) >= 80) {
    status_ |= MODE_DATA_TRANSFER_BITS;
  } else {
    status_ |= MODE_OAM_SEARCH_BITS;
  }

  // TODO: LCDSTAT interrupts

  int line_number = LCDCY();

  // Determine whether it's time to render this scanline.
  // Scanlines are rendered a certain number of cycles into a particular LCD
  // line. That number depends on the number of sprites present in the row. I
  // don't know the exact number, but I know that the upper bound on the
  // number of cycles is 297, so wait until we are at least 297 cycles into
  // the scanline before transferring it to the framebuffer.
  if ((previous_scanline_ < line_number ||
       (line_number <= 0 && previous_scanline_ >= 143)) &&
      line_number < 144 && (cycle_clock_ % kLineCycleCount) > 83) {
    previous_scanline_ = line_number;

    // Render the current scanline out to the framebuffer.
    RenderScanline();
  }

  // Each time the LCD Y-coordinate advances, render the next line to the
  // display.
  if (line_number != y_compare_) {
    y_compare_ = line_number;

    // Check to see if LY == LYC.
    if (line_number == lyc_) {
      status_ |= COINCIDENCE_FLAG_BIT;
      // If the coincidence interrupt is enabled, signal an LCD_STAT
      // interrupt.
      if ((status_ & COINCIDENCE_INTERRUPT_BIT) != 0) {
        interrupts_->SignalInterrupt(INTERRUPT_LCD_STAT);
      }
    } else {
      status_ &= (~COINCIDENCE_FLAG_BIT);
    }

    // Check if we've just entered line 144, or began the VBlank.
    if (line_number == 144) {
      interrupts_->SignalInterrupt(INTERRUPT_VBLANK);

      // Tell the window controller to draw the frame to the screen.
      window_controller_->SignalFrame();
    }
  }
}

void Display::SetControl(uint8_t value) {
  // TODO: turn off display when bit 7 goes to 0, but only during VBLANK
  if ((value & OBJ_SIZE_BIT) != 0) {
    FATALF("Unimplemented control bit: sprite size");
  }
  control_ = value;
}

void Display::SetStatus(uint8_t value) {
  if ((value & (~(COINCIDENCE_INTERRUPT_BIT))) != 0) {
    FATALF("Unimplemented status bits: 0x%02x", value);
  }
  // Bits 0-2 are read-only.
  status_ = (value & 0xf8) | (status_ & 0x07);
}

uint32_t Display::Color(uint8_t value) {
  int shift = value*2;
  uint8_t idx = (palette_ >> shift) & 0x3;
  switch (idx) {
  case 0: return 0xffffffff;
  case 1: return 0xaaaaaaaa;
  case 2: return 0x44444444;
  }
  return 0;
}

Tile *Display::GetTile(int x, int y, bool use_first_tilemap) {
  CHECK(0 <= x && x <= 32);
  CHECK(0 <= y && y <= 32);
  // TODO: this mutex lock doesn't actually protect against accessing the
  // tile's memory after this function returns.
  // TODO: this mutex seems to be slowing things down A LOT, probably because
  // it's called at every pixel.
  //std::lock_guard<std::mutex> lock(mutex_);
  uint8_t tile_id = (use_first_tilemap)
      ? tilemap_[y*32 + x]
      : tilemap_[y*32 + x + 0x400];
  if ((control_ & TILE_DATA_SELECT_BIT) == 0) {
    // In this case, the tile_id is interpreted as a signed value, and 0x9000
    // is tile_id = 0.
    int8_t signed_tile_id = reinterpret_cast<int8_t&>(tile_id);
    return reinterpret_cast<struct Tile*>(
        vram_ + 0x1000 + (signed_tile_id * 16));
  }
  return reinterpret_cast<struct Tile*>(vram_ + (tile_id * 16));
}

uint8_t Display::Read8(uint16_t offset) {
  CHECK(0 <= offset && offset <= kVRAMSize - 1);
  std::lock_guard<std::mutex> lock(mutex_);
  return vram_[offset];
}

void Display::Write8(uint16_t offset, uint8_t value) {
  CHECK(0 <= offset && offset <= kVRAMSize - 1);
  std::lock_guard<std::mutex> lock(mutex_);
  vram_[offset] = value;
}

void Display::WriteSprite8(uint16_t offset, uint8_t value) {
  CHECK(0 <= offset && offset <= kSpriteAttributeTableSize);
  sprite_attributes_[offset] = value;
}

uint8_t Display::ReadSprite8(uint16_t offset) {
  CHECK(0 <= offset && offset <= kSpriteAttributeTableSize);
  return sprite_attributes_[offset];
}

void Display::Loop() {
  // Draw the tileset out to the second window.
  tileset_window_->SetRenderFunc([this]{
    int tx, ty;
    for (int t = 0; t < 384; ++t) {
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

  background_window_->SetRenderFunc([this]{
    int tile_y, tile_x;
    for (int y = 0; y < 256; ++y) {
      tile_y = y % 8;
      for (int x = 0; x < 256; ++x) {
        tile_x = x % 8;

        Tile *tile = this->GetTile(x/8, y/8,
            ((control_ & BG_TILE_MAP_SELECT_BIT) == 0));
        uint8_t *b = tile->data + (tile_y*2);
        uint8_t value =
          ((*b >> (7-tile_x)) & 0x1) | (((*(b+1) >> (7-tile_x)) & 0x1) << 1);

        this->background_window_->SetPixel(x, y, this->Color(value));
      }
    }
  });

  // Loop.
  while (window_controller_->Tick()) {}
}

void Display::RenderScanline() {
  // TODO: check if LCD is on

  uint8_t line_index = LCDCY();
  if (line_index >= kDisplayHeight) return;

  // The background tile map dimensions, in pixels.
  const int kBackgroundMapWidth = 256;
  const int kBackgroundMapHeight = 256;

  // Used for composition, to determine if one color should overwrite another.
  int color_indices[kDisplayWidth];

  // This lock will be destroyed and released when the function returns.
  auto pixel_lock = window_->LockPixels();

  // Render background tiles.
  if ((control_ & BG_ENABLE_BIT) != 0) {
    int offset_x = ScrollX();
    int offset_y = ScrollY();

    // Wrap BG tile map.
    int y = (offset_y + line_index) % kBackgroundMapHeight;
    int tile_y = y % 8;

    int x, tile_x;
    for (int ix = 0; ix < kDisplayWidth; ++ix) {
      // Wrap BG tile map.
      x = (offset_x + ix) % kBackgroundMapWidth;
      tile_x = x % 8;

      // TODO: factor out into a function
      Tile *tile = this->GetTile(x/8, y/8,
          ((control_ & BG_TILE_MAP_SELECT_BIT) == 0));
      uint8_t *b = tile->data + (tile_y*2);
      uint8_t value =
        ((*b >> (7-tile_x)) & 0x1) | (((*(b+1) >> (7-tile_x)) & 0x1) << 1);

      color_indices[ix] = value;
      this->window_->SetPixel(ix, line_index, this->Color(value));
    }
  }

  // Render windows.
  if ((control_ & WINDOW_ENABLE_BIT) != 0) {
    int offset_x = WindowX() - 7;
    int offset_y = WindowY();

    // Check if the window overlaps with this scanline.
    if (offset_y <= line_index && line_index < offset_y + kDisplayHeight &&
        -7 <= offset_x && offset_x < 159) {
      int screen_x, tile_x;
      int window_y = line_index - offset_y;
      int tile_y = window_y % 8;
      for (int window_x = 0; window_x < kDisplayWidth; ++window_x) {
        screen_x = offset_x + window_x;
        if (screen_x < 0 || kDisplayWidth < screen_x) {
          continue;
        }
        tile_x = window_x % 8;

        Tile *tile = this->GetTile(window_x/8, window_y/8,
            ((control_ & WINDOW_TILE_MAP_SELECT_BIT) == 0));
        uint8_t *b = tile->data + (tile_y*2);
        uint8_t value =
          ((*b >> (7-tile_x)) & 0x1) | (((*(b+1) >> (7-tile_x)) & 0x1) << 1);

        color_indices[screen_x] = value;
        this->window_->SetPixel(screen_x, line_index, this->Color(value));
      }
    }
  }

  // Render sprites.
  if ((control_ & OBJ_ENABLE_BIT) != 0) {
    for (int i = 0; i < 40; ++i) {
      Sprite *sprite =
          reinterpret_cast<Sprite*>(this->sprite_attributes_ + (4*i));
      // Check if the sprite overlaps with this scanline.
      if (sprite->y_pos() <= line_index || line_index < sprite->y_pos() - 16 ||
          sprite->x_pos() == 0 || sprite->x_pos() >= 168) {
        continue;
      }
      Tile *tile = reinterpret_cast<struct Tile*>(
          this->vram_ + (sprite->tile_id() * 16));

      // TODO: refactor, this is ugly
      int x, window_x;
      int iy = line_index - (sprite->y_pos() - 16);
      if (iy > 7) continue;
      int y = ((sprite->attributes() & SPRITE_Y_FLIP_BIT) == 0) ? iy : (7-iy);
      for (int ix = 0; ix < 8; ++ix) {
        x = ((sprite->attributes() & SPRITE_X_FLIP_BIT) == 0) ? ix : (7-ix);
        uint8_t *b = tile->data + (y*2);
        uint8_t value =
            ((*b >> (7-x)) & 0x1) | (((*(b+1) >> (7-x)) & 0x1) << 1);
        if (value == 0) {
          continue;  // For sprites, color 0 is transparent.
        }

        // TODO: use sprite palettes instead
        window_x = (sprite->x_pos() + ix - 8);
        if ((sprite->attributes() & SPRITE_OBJ_TO_BG_PRIORITY_BIT) != 0 &&
            color_indices[window_x] != 0) {
          // Skip this pixel because BG priority is enabled and the background
          // color is not 0.
          continue;
        }
        this->window_->SetPixel(
            window_x,
            (sprite->y_pos() + iy - 16),
            this->Color(value));
      }
    }
  }
}

}  // namespace dgb
