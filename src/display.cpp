#include "display.h"

namespace dgb {

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

  // Loop.
  while (window_controller_->Tick()) {}
}

void Display::RenderScanline() {
  // TODO: check if LCD is on

  uint8_t line_index = LCDCY();
  if (line_index > kDisplayHeight) return;

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
