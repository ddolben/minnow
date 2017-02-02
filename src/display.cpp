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
      Tile *tile = this->GetTile(x/8, y/8);
      uint8_t *b = tile->data + (tile_y*2);
      uint8_t value =
        ((*b >> (7-tile_x)) & 0x1) | (((*(b+1) >> (7-tile_x)) & 0x1) << 1);
      this->window_->SetPixel(ix, line_index, this->Color(value));
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
      int x, y;
      int iy = line_index - (sprite->y_pos() - 16);
      if (iy > 7) continue;
      int y = ((sprite->attributes() & SPRITE_Y_FLIP_BIT) == 0) ? iy : (7-iy);
      for (int ix = 0; ix < 8; ++ix) {
        x = ((sprite->attributes() & SPRITE_X_FLIP_BIT) == 0) ? ix : (7-ix);
        uint8_t *b = tile->data + (y*2);
        uint8_t value =
            ((*b >> (7-x)) & 0x1) | (((*(b+1) >> (7-x)) & 0x1) << 1);
        // TODO: use sprite palettes instead
        // TODO: some colors are transparent
        this->window_->SetPixel(
            (sprite->x_pos() + ix - 8),
            (sprite->y_pos() + iy - 16),
            this->Color(value));
      }
    }
  }
}

}  // namespace dgb
