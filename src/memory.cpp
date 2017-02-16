#include "memory.h"

#include <cstdint>
#include <string>

namespace dgb {

void Memory::LoadBootloader(const std::string &filename) {
  bootstrap_.reset(new MMapFile(filename));
  bootstrap_is_mapped_ = true;
}

uint8_t Memory::Read8(uint16_t offset) {
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

// TODO: merge with above
uint8_t *Memory::GetOffset(uint16_t offset) {
  if (bootstrap_is_mapped_ && offset <= 0xff) {  // Bootstrap code.
    return reinterpret_cast<uint8_t*>(bootstrap_->memory()) + offset;
  } else if (offset < 0x8000) {  // ROM

  } else if (offset < 0xA000) {  // Video RAM
    // Fall through (this should never happen)
  } else if (offset < 0xC000) {  // Cartridge RAM

  } else if (offset < 0xE000) {  // Working RAM
    return &wram_[offset - 0xC000];
  } else if (offset < 0xFE00) {  // Echo of working RAM C000-DDFF
    return &wram_[offset - 0xE000];
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

void Memory::Write8(uint16_t offset, uint8_t value) {
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

// TODO: merge with above
void Memory::Write(uint16_t offset, uint8_t *value, int length) {
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

uint8_t Memory::ReadFromDevice(uint16_t offset) {
  if (offset == 0xff00) { return input_->Joypad(); }
  if (offset == 0xff04) { return timers_->Divider(); }
  if (offset == 0xff0f) { return interrupt_flag_; }

  if (offset == 0xff40) { return display_->Control(); }
  if (offset == 0xff41) { return display_->Status(); }
  if (offset == 0xff42) { return display_->ScrollY(); }
  if (offset == 0xff43) { return display_->ScrollX(); }
  if (offset == 0xff44) { return display_->LCDCY(); }
  if (offset == 0xff45) { return display_->LYC(); }
  if (offset == 0xff4a) { return display_->WindowY(); }
  if (offset == 0xff4b) { return display_->WindowX(); }

  if (offset == 0xffff) { return interrupt_enable_; }

  FATALF("NOT IMPLEMENTED: read from device 0x%04x", offset & 0xffff);
}

void Memory::WriteToDevice(uint16_t offset, uint8_t value) {
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
    timers_->set_modulo(value);
    return;
  }
  if (offset == 0xff07) {  // Timer Control register.
    timers_->set_control(value);
    return;
  }
  if (offset == 0xff0f) {  // Interrupt Flag register.
    interrupt_flag_ = value;
    return;
  }
  if ((0xff10 <= offset && offset <= 0xff26) ||
      (0xff30 <= offset && offset <= 0xff3f)) {  // Sound control registers.
    //WARNINGF("NOT IMPLEMENTED: write to sound device (0x%04x) <- 0x%04x",
    //    offset & 0xffff, value & 0xff);
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
    } else if (offset == 0xff45) {
      display_->SetLYC(value);
    } else if (offset == 0xff47) {
      display_->SetPalette(value);
    } else if (offset == 0xff48) {
      display_->SetObjectPalette0(value);
    } else if (offset == 0xff49) {
      display_->SetObjectPalette1(value);
    } else if (offset == 0xff4a) {
      display_->SetWindowY(value);
    } else if (offset == 0xff4b) {
      display_->SetWindowX(value);
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
  if (offset == 0xff7c || offset == 0xff7d || offset == 0xff7e || offset == 0xff7f) {
    // TODO
    ERRORF("NOT IMPLEMENTED: write to device (0x%04x) <- 0x%04x",
        offset & 0xffff, value & 0xff);
    return;
  }
  if (offset == 0xffff) {
    interrupt_enable_ = value;
    return;
  }
  // TODO
  ERRORF("NOT IMPLEMENTED: write to device (0x%04x) <- 0x%04x",
      offset & 0xffff, value & 0xff);
}

void Memory::StartDMATransfer(uint8_t value) {
  // TODO: do this in a separate thread?
  // TODO: restrict CPU access to only HRAM?
  uint16_t src_start = value << 8;
  uint16_t dest_start = 0xFE00;
  for (uint8_t offset = 0; offset < 0xA0; ++offset) {
    // TODO: write directly to sprite attribute table
    Write8(dest_start + offset, Read8(src_start + offset));
  }
}

}  // namespace dgb
