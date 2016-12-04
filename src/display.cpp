#include "display.h"

namespace dgb {

void Display::Loop() {
  // Draw the tilemap into the main window.
  window_->SetRenderFunc([this]{
    // TODO: check if LCD is on

    // Render background tile map.
    if ((control_ & BG_ENABLE_BIT) != 0) {
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
    }

    // TODO: render window

    // Render sprites.
    if ((control_ & OBJ_ENABLE_BIT) != 0) {
      for (int i = 0; i < 40; ++i) {
        Sprite *sprite = reinterpret_cast<Sprite*>(this->sprite_attributes_ + (4*i));
        if (sprite->y_pos() == 0 || sprite->y_pos() >= 160 ||
            sprite->x_pos() == 0 || sprite->x_pos() >= 168) {
          continue;
        }
        Tile *tile = reinterpret_cast<struct Tile*>(
            this->vram_ + (sprite->tile_id() * 16));
        for (int y = 0; y < 8; ++y) {
          for (int x = 0; x < 8; ++x) {
            uint8_t *b = tile->data + (y*2);
            uint8_t value = ((*b >> (7-x)) & 0x1) | (((*(b+1) >> (7-x)) & 0x1) << 1);
            // TODO: use sprite palettes instead
            // TODO: some colors are transparent
            this->window_->SetPixel(
                (sprite->x_pos() + x - 8),
                (sprite->y_pos() + y - 16),
                this->Color(value));
          }
        }
      }
    }
  });

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

}  // namespace dgb
