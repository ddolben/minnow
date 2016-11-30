#ifndef DGB_MEMORY_H_
#define DGB_MEMORY_H_

#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

#include "cartridge.h"
#include "display.h"
#include "input.h"
#include "logging.h"
#include "mmapfile.h"


namespace dgb {

// TODO: rename to "Bus" or "MemoryBus"
// Memory represents the Game Boy's internal address space. It doesn't all
// represent direct memory access, and in fact might be more accurately
// described as a memory bus. The address space is divided as follows:
//
//   0x0000-0x3FFF: Permanently-mapped ROM bank.
//   0x4000-0x7FFF: Area for switchable ROM banks.
//   0x8000-0x9FFF: Video RAM.
//   0xA000-0xBFFF: Area for switchable external RAM banks.
//   0xC000-0xCFFF: Game Boy’s working RAM bank 0 .
//   0xD000-0xDFFF: Game Boy’s working RAM bank 1.
//   0xFE00-0xFEFF: Sprite Attribute Table.
//   0xFF00-0xFF7F: Devices’ Mappings. Used to access I/O devices.
//   0xFF80-0xFFFE: High RAM Area.
//   0xFFFF: Interrupt Enable Register.
//
class Memory {
 public:
  Memory(const std::string &bootstrap, std::shared_ptr<Cartridge> cartridge,
      std::shared_ptr<Display> display, std::shared_ptr<Input> input)
    : bootstrap_(new MMapFile(bootstrap)), cartridge_(cartridge),
      display_(display), input_(input) {}

  uint8_t Read8(uint16_t offset) {
    if (bootstrap_is_mapped_ && offset <= 0xff) {  // Bootstrap code.
      return *(reinterpret_cast<uint8_t*>(bootstrap_->memory()) + offset);
    }
    if (offset < 0x8000) return cartridge_->Read8(offset);  // ROM
    if (offset < 0xA000) return display_->Read8(offset - 0x8000);  // VRAM
    if (offset < 0xC000) return cartridge_->Read8(offset);  // Cartridge RAM
    if (0xFE00 <= offset && offset <= 0xFE9F)
      return display_->ReadSprite8(offset - 0xFE00);
    if ((0xFF00 <= offset && offset < 0xFF80) || offset == 0xFFFF) {
      return ReadFromDevice(offset);
    }
    return *GetOffset(offset);
  }

  uint16_t Read16(uint16_t offset) {
    if (bootstrap_is_mapped_ && offset <= 0xff) {  // Bootstrap code.
      return *reinterpret_cast<uint16_t*>(
          reinterpret_cast<uint8_t*>(bootstrap_->memory()) + offset);
    }
    if (offset < 0x8000) return cartridge_->Read16(offset);  // ROM
    if (offset < 0xA000) return display_->Read16(offset - 0x8000);  // VRAM
    if (offset < 0xC000) return cartridge_->Read16(offset);  // Cartridge RAM
    if (0xFE00 <= offset && offset <= 0xFE9F)
      // TODO: 16-bit
      return display_->ReadSprite8(offset - 0xFE00);
    if ((0xFF00 <= offset && offset < 0xFF80) || offset == 0xFFFF) {
      // TODO: 16-bit???
      WARNINGF("Reading 16 bits from device address 0x%04x might be buggy",
          offset);
      return ReadFromDevice(offset);
    }
    return *reinterpret_cast<uint16_t*>(GetOffset(offset));
  }

  void Write8(uint16_t offset, uint8_t value) {
    if (offset < 0x8000) return cartridge_->Write8(offset, value);  // ROM
    if (offset < 0xA000) {
      return display_->Write8(offset - 0x8000, value);  // VRAM
    }
    if (offset < 0xC000) return cartridge_->Write8(offset, value);  // Cartridge RAM
    // Sprite Attribute Table
    if (0xFE00 <= offset && offset <= 0xFE9F)
      return display_->WriteSprite8(offset - 0xFE00, value);
    if (0xFEA0 <= offset && offset <= 0xFEFF) {
      WARNINGF("Noop memory write: (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      return;
    }
    if (offset == 0xFF46) return StartDMATransfer(value);
    Write(offset, reinterpret_cast<uint8_t*>(&value), 1);
  }

  void Write16(uint16_t offset, uint16_t value) {
    if (offset < 0x8000) return cartridge_->Write16(offset, value);  // ROM
    if (offset < 0xA000) return display_->Write16(offset - 0x8000, value);  // VRAM
    if (offset < 0xC000) return cartridge_->Write16(offset, value);  // Cartridge RAM
    if (0xFEA0 <= offset && offset <= 0xFEFF) {
      WARNINGF("Noop memory write: (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      return;
    }
    Write(offset, reinterpret_cast<uint8_t*>(&value), 2);
  }

  void set_bootstrap_is_mapped(bool value) { bootstrap_is_mapped_ = value; }
  bool bootstrap_is_mapped() { return bootstrap_is_mapped_; }

 private:
  uint8_t *GetOffset(uint16_t offset) {
    if (bootstrap_is_mapped_ && offset <= 0xff) {  // Bootstrap code.
      return reinterpret_cast<uint8_t*>(bootstrap_->memory()) + offset;
    } else if (offset < 0x8000) {  // ROM

		} else if (offset < 0xA000) {  // Video RAM
      // Fall through (this should never happen)
    } else if (offset < 0xC000) {  // Cartridge RAM

    } else if (offset < 0xE000) {  // Working RAM
      return &wram_[offset - 0xC000];
    } else if (offset < 0xFF80) {
      // Fall through
    } else if (offset < 0xFFFF) {  // High RAM Area.
      return &high_ram_[offset - 0xFF80];
    } else {  // Interrupt enable register.
      // Fall through.
    }

    FATALF("NOT IMPLEMENTED: read from address 0x%04x", 0xffff & offset);
    return nullptr;  // Unreachable.
  }

  void Write(uint16_t offset, uint8_t *value, int length) {
		if (offset < 0x8000) {  // Cartridge ROM

		} else if (offset < 0xA000) {  // Video RAM
      for (int i = 0; i < length; ++i) {
        display_->Write8(offset - 0x8000 + i, *(value + i));
      }
      return;
    } else if (offset < 0xC000) {  // Cartridge RAM
      // Fall through
    } else if (offset < 0xE000) {  // Working RAM
      for (int i = 0; i < length; ++i) {
        wram_[offset - 0xC000 + i] = *(value + i);
      }
      return;
    } else if (offset < 0xFF00) {
      // Fall through
    } else if (offset < 0xFF80) {  // Device registers.
      for (int i = 0; i < length; ++i) {
        WriteToDevice(offset + i, *(value + i));
      }
      return;
    } else if (offset < 0xFFFF) {  // High RAM Area.
      for (int i = 0; i < length; ++i) {
        high_ram_[offset - 0xFF80 + i] = *(value + i);
      }
      return;
    } else {  // Interrupt enable register.
      for (int i = 0; i < length; ++i) {
        WriteToDevice(offset + i, *(value + i));
      }
      return;
    }

    FATALF("NOT IMPLEMENTED: write %d bytes (0x%04x) <- 0x%04x\n",
        length, 0xffff & offset, 0xffff & *reinterpret_cast<uint16_t*>(value));
  }

  uint8_t ReadFromDevice(uint16_t offset) {
    if (offset == 0xff00) {
      return input_->Joypad();
    }
    if (offset == 0xff0f) {  // Interrupt Flag register.
      return interrupt_flag_;
    }
    if (offset == 0xff40) {
      return display_->Control();
    }
    if (offset == 0xff42) {
      return display_->ScrollY();
    }
    if (offset == 0xff43) {
      return display_->ScrollX();
    }
    if (offset == 0xff44) {
      return display_->LCDCY();
    }
    if (offset == 0xffff) {  // Interrupt Enable register.
      return interrupt_enable_;
    }
    FATALF("NOT IMPLEMENTED: read from device 0x%04x", offset & 0xffff);
  }

  void WriteToDevice(uint16_t offset, uint8_t value) {
    if (offset == 0xff00) {
      input_->SetJoypad(value);
      return;
    }
    if (offset == 0xff01) {  // Serial Transfer Data register.
      WARNINGF("NOT IMPLEMENTED: write to Serial Transfer Data register "
          "(0xff01) <- 0x%02x", value);
      return;
    }
    if (offset == 0xff02) {  // Serial Transfer Control register.
      WARNINGF("NOT IMPLEMENTED: write to Serial Transfer Control register "
          "(0xff02) <- 0x%02x", value);
      return;
    }
    if (offset == 0xff06) {  // Timer Modulo register.
      ERRORF("NOT IMPLEMENTED: timers (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      timer_modulo_ = value;
      return;
    }
    if (offset == 0xff07) {  // Timer Control register.
      ERRORF("NOT IMPLEMENTED: timers (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      timer_control_ = value;
      return;
    }
    if (offset == 0xff0f) {  // Interrupt Flag register.
      ERRORF("NOT IMPLEMENTED: interrupts (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      interrupt_flag_ = value;
      return;
    }
    if (0xff10 <= offset && offset <= 0xff26) {  // Sound control registers.
      WARNINGF("NOT IMPLEMENTED: write to sound device (0x%04x) <- 0x%04x",
          offset & 0xffff, value & 0xff);
      return;
    }
    if (0xff40 <= offset && offset <= 0xff4b) {  // Display IO registers.
      if (offset == 0xff40) {
        display_->SetControl(value);
      } else if (offset == 0xff41) {
        display_->SetStatus(value);
      } else if (offset == 0xff42) {
        display_->SetScrollY(value);
      } else if (offset == 0xff43) {
        display_->SetScrollX(value);
      } else if (offset == 0xff47) {
        display_->SetPalette(value);
      } else if (offset == 0xff48) {
        display_->SetObjectPalette0(value);
      } else if (offset == 0xff49) {
        display_->SetObjectPalette1(value);
      } else if (offset == 0xff4a) {
        ERRORF("NOT IMPLEMENTED: windows (0x%04x) <- 0x%02x",
            offset & 0xffff, value & 0xff);
        display_->SetWindowY(value);
      } else if (offset == 0xff4b) {
        // For some reason, this register is Window X minus 7
        // TODO: be careful of over- and underflow.
        ERRORF("NOT IMPLEMENTED: windows (0x%04x) <- 0x%02x",
            offset & 0xffff, value & 0xff);
        display_->SetWindowX(value - 7);
      } else {
        FATALF("NOT IMPLEMENTED: write to video device (0x%04x) <- %s",
            offset & 0xffff, ByteBits(value).c_str());
      }
      return;
    }
    if (offset == 0xff50 && value == 1) {  // Unmap the boot rom.
      bootstrap_is_mapped_ = false;
      return;
    }
    if (offset == 0xff7f) {
      // TODO
      ERRORF("NOT IMPLEMENTED: write to device (0x%04x) <- 0x%04x",
          offset & 0xffff, value & 0xff);
      return;
    }
    if (offset == 0xffff) {
      ERRORF("NOT IMPLEMENTED: interrupts (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      interrupt_enable_ = value;
      return;
    }
    // TODO
    FATALF("NOT IMPLEMENTED: write to device (0x%04x) <- 0x%04x",
        offset & 0xffff, value & 0xff);
  }

  void StartDMATransfer(uint8_t value);

  bool bootstrap_is_mapped_ = true;
  std::unique_ptr<MMapFile> bootstrap_;
  uint8_t wram_[8192];  // 8k of working ram
  uint8_t high_ram_[128];  // 128 bytes of high ram

  uint8_t timer_modulo_ = 0;  // 0xFF06
  uint8_t timer_control_ = 0;  // 0xFF07

  // 0xFF0F
  // Interrupt Enable register (bits are set to enable interrupts).
  //   Bit 0: V-Blank  Interrupt Enable  (INT 40h)  (1=Enable)
  //   Bit 1: LCD STAT Interrupt Enable  (INT 48h)  (1=Enable)
  //   Bit 2: Timer    Interrupt Enable  (INT 50h)  (1=Enable)
  //   Bit 3: Serial   Interrupt Enable  (INT 58h)  (1=Enable)
  //   Bit 4: Joypad   Interrupt Enable  (INT 60h)  (1=Enable)
  uint8_t interrupt_enable_ = 0;

  // 0xFFFF
  // Interrupt Flag register (bits are set according to interrupt signals).
  //   Bit 0: V-Blank  Interrupt Request (INT 40h)  (1=Request)
  //   Bit 1: LCD STAT Interrupt Request (INT 48h)  (1=Request)
  //   Bit 2: Timer    Interrupt Request (INT 50h)  (1=Request)
  //   Bit 3: Serial   Interrupt Request (INT 58h)  (1=Request)
  //   Bit 4: Joypad   Interrupt Request (INT 60h)  (1=Request)
  uint8_t interrupt_flag_ = 0;

  std::shared_ptr<Cartridge> cartridge_;
  std::shared_ptr<Display> display_;
  std::shared_ptr<Input> input_;
};

}  // namespace dgb

#endif  // DGB_MEMORY_H_
