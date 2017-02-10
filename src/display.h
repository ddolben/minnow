#ifndef DGB_DISPLAY_H_
#define DGB_DISPLAY_H_

#include <functional>
#include <memory>
#include <mutex>

#include "clock.h"
#include "interrupts.h"
#include "logging.h"
#include "window.h"


namespace dgb {

// Used to apply structure on top of tile memory.
struct Tile {
  uint8_t data[16];
};

// Used to apply structure on top of sprite attribute memory.
struct Sprite {
  uint8_t data[4];

  // Y position minus 16
  uint8_t y_pos() { return data[0]; }
  // X position minus 8
  uint8_t x_pos() { return data[1]; }
  // Index into the first tileset (0x8000-0x8FFF)
  uint8_t tile_id() { return data[2]; }
  // Bit7   OBJ-to-BG Priority (0=OBJ Above BG, 1=OBJ Behind BG color 1-3)
  //        (Used for both BG and Window. BG color 0 is always behind OBJ)
  // Bit6   Y flip          (0=Normal, 1=Vertically mirrored)
  // Bit5   X flip          (0=Normal, 1=Horizontally mirrored)
  // Bit4   Palette number  **Non CGB Mode Only** (0=OBP0, 1=OBP1)
  // Bit3   Tile VRAM-Bank  **CGB Mode Only**     (0=Bank 0, 1=Bank 1)
  // Bit2-0 Palette number  **CGB Mode Only**     (OBP0-7)
  uint8_t attributes() { return data[3]; }
};

// Class representing the display device.
class Display {
 public:
  // Bit 7 - LCD Display Enable             (0=Off, 1=On)
  // Bit 6 - Window Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
  // Bit 5 - Window Display Enable          (0=Off, 1=On)
  // Bit 4 - BG & Window Tile Data Select   (0=8800-97FF, 1=8000-8FFF)
  // Bit 3 - BG Tile Map Display Select     (0=9800-9BFF, 1=9C00-9FFF)
  // Bit 2 - OBJ (Sprite) Size              (0=8x8, 1=8x16)
  // Bit 1 - OBJ (Sprite) Display Enable    (0=Off, 1=On)
  // Bit 0 - BG Display (for CGB see below) (0=Off, 1=On)
  enum ControlMask {
    WINDOW_TILE_MAP_SELECT_BIT = 0x40,
    WINDOW_ENABLE_BIT = 0x20,
    TILE_DATA_SELECT_BIT = 0x10,
    BG_TILE_MAP_SELECT_BIT = 0x08,
    OBJ_SIZE_BIT = 0x04,
    OBJ_ENABLE_BIT = 0x02,
    BG_ENABLE_BIT = 0x01
  };

	// Bit 6 - LYC=LY Coincidence Interrupt (1=Enable) (Read/Write)
  // Bit 5 - Mode 2 OAM Interrupt         (1=Enable) (Read/Write)
  // Bit 4 - Mode 1 V-Blank Interrupt     (1=Enable) (Read/Write)
  // Bit 3 - Mode 0 H-Blank Interrupt     (1=Enable) (Read/Write)
  // Bit 2 - Coincidence Flag  (0:LYC<>LY, 1:LYC=LY) (Read Only)
  // Bit 1-0 - Mode Flag       (Mode 0-3, see below) (Read Only)
  // 					0: During H-Blank
  // 					1: During V-Blank
  // 					2: During Searching OAM-RAM
  // 					3: During Transfering Data to LCD Driver
  enum StatusMask {
    COINCIDENCE_INTERRUPT_BIT = 0x40,
    COINCIDENCE_FLAG_BIT = 0x04,
    MODE_DATA_TRANSFER_BITS = 0x03,
    MODE_OAM_SEARCH_BITS = 0x02,
    MODE_VBLANK_BITS = 0x01,
    MODE_HBLANK_BITS = 0x00
  };

	// Bit7   OBJ-to-BG Priority (0=OBJ Above BG, 1=OBJ Behind BG color 1-3)
  //        (Used for both BG and Window. BG color 0 is always behind OBJ)
  // Bit6   Y flip          (0=Normal, 1=Vertically mirrored)
  // Bit5   X flip          (0=Normal, 1=Horizontally mirrored)
  // Bit4   Palette number  **Non CGB Mode Only** (0=OBP0, 1=OBP1)
  // Bit3   Tile VRAM-Bank  **CGB Mode Only**     (0=Bank 0, 1=Bank 1)
  // Bit2-0 Palette number  **CGB Mode Only**     (OBP0-7)
  enum SpriteAttributeMask {
    SPRITE_OBJ_TO_BG_PRIORITY_BIT = 0x80,
    SPRITE_Y_FLIP_BIT = 0x40,
    SPRITE_X_FLIP_BIT = 0x20
  };

  const static uint16_t kVRAMSize = 8192;
  const static uint16_t kSpriteAttributeTableSize = 0xA0;
  const static int kCycleLength = 70224;
  const static int kVisibleLineCount = 144;
  const static int kLineCycleCount = 456;
  const static int kVBlankStart = kVisibleLineCount * kLineCycleCount;
  const static int kVBlankCycleCount = 4560;

  static const int kDisplayWidth = 160;
  static const int kDisplayHeight = 144;

  Display(int width, int height, std::shared_ptr<Clock> clock,
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

  void AdvanceClock(int cycles) {
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
    // Each time the LCD Y-coordinate advances, render the next line to the
    // display.
    if (line_number != y_compare) {
      y_compare = line_number;

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

      // Render the current scanline out to the framebuffer.
      RenderScanline();

      // Check if we've just entered line 144, or began the VBlank.
      if (line_number == 144) {
        interrupts_->SignalInterrupt(INTERRUPT_VBLANK);

        // Tell the window controller to draw the frame to the screen.
        window_controller_->SignalFrame();
      }
    }
  }

  // LCDC Y-coordinate value.
  uint8_t LCDCY() {
    return cycle_clock_ / kLineCycleCount;
  }

  // LY Compare value.
  uint8_t LYC() { return lyc_; }
  void SetLYC(uint8_t value) {
    lyc_ = value;
  }

  uint8_t Control() { return control_; }
  void SetControl(uint8_t value) {
    // TODO: turn off display when bit 7 goes to 0, but only during VBLANK
    if ((value & OBJ_SIZE_BIT) != 0) {
      FATALF("Unimplemented control bit: sprite size");
    }
    control_ = value;
  }

  uint8_t Status() { return status_; }
  void SetStatus(uint8_t value) {
    if ((value & (~(COINCIDENCE_INTERRUPT_BIT))) != 0) {
      FATALF("Unimplemented status bits: 0x%02x", value);
    }
    // Bits 0-2 are read-only.
    status_ = (value & 0xf8) | (status_ & 0x07);
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
  Tile *GetTile(int x, int y, bool use_first_tilemap) {
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

  uint8_t Read8(uint16_t offset) {
    CHECK(0 <= offset && offset <= kVRAMSize - 1);
    std::lock_guard<std::mutex> lock(mutex_);
    return vram_[offset];
  }

  void Write8(uint16_t offset, uint8_t value) {
    CHECK(0 <= offset && offset <= kVRAMSize - 1);
    std::lock_guard<std::mutex> lock(mutex_);
    vram_[offset] = value;
  }

  void WriteSprite8(uint16_t offset, uint8_t value) {
    CHECK(0 <= offset && offset <= kSpriteAttributeTableSize);
    sprite_attributes_[offset] = value;
  }

  uint8_t ReadSprite8(uint16_t offset) {
    CHECK(0 <= offset && offset <= kSpriteAttributeTableSize);
    return sprite_attributes_[offset];
  }

  void Loop();

 private:
  // Renders the current scanline (determined by LCDCY()) and writes it out to
  // the window's framebuffer.
  void RenderScanline();

  int y_compare = 0;
  uint8_t control_ = 0;
  uint8_t status_ = 0;
  // LY Compare value.
  uint8_t lyc_ = 0;

  uint8_t palette_ = 0;
  uint8_t object_palette_0_ = 0;
  uint8_t object_palette_1_ = 0;

  std::mutex mutex_;
  uint8_t vram_[kVRAMSize];
  uint8_t *tilemap_ = vram_ + 0x1800;
  uint8_t sprite_attributes_[kSpriteAttributeTableSize];

  // Used to figure out where the display is in its cycle.
  int cycle_clock_ = 0;

  std::shared_ptr<Interrupts> interrupts_;
  std::shared_ptr<WindowController> window_controller_;

  std::shared_ptr<Window> window_;
  std::shared_ptr<Window> background_window_;
  std::shared_ptr<Window> tileset_window_;
};

}  // namespace dgb

#endif  // DGB_DISPLAY_H_
