#include "cartridge.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>

#include "clock.h"
#include "common/logging.h"
#include "common/file/fileutils.h"
#include "common/time/time.h"

namespace dgb {

namespace {

const uint16_t kCartridgeTypeAddress = 0x147;

const std::map<int, std::string> kCartridgeTypes = {
  {0x00, "ROM ONLY"},
  {0x01, "MBC1"},
  {0x02, "MBC1+RAM"},
  {0x03, "MBC1+RAM+BATTERY"},
  {0x05, "MBC2"},
  {0x06, "MBC2+BATTERY"},
  {0x08, "ROM+RAM"},
  {0x09, "ROM+RAM+BATTERY"},
  {0x0B, "MMM01"},
  {0x0C, "MMM01+RAM"},
  {0x0D, "MMM01+RAM+BATTERY"},
  {0x0F, "MBC3+TIMER+BATTERY"},
  {0x10, "MBC3+TIMER+RAM+BATTERY"},
  {0x11, "MBC3"},
  {0x12, "MBC3+RAM"},
  {0x13, "MBC3+RAM+BATTERY"},
  {0x15, "MBC4"},
  {0x16, "MBC4+RAM"},
  {0x17, "MBC4+RAM+BATTERY"},
  {0x19, "MBC5"},
  {0x1A, "MBC5+RAM"},
  {0x1B, "MBC5+RAM+BATTERY"},
  {0x1C, "MBC5+RUMBLE"},
  {0x1D, "MBC5+RUMBLE+RAM"},
  {0x1E, "MBC5+RUMBLE+RAM+BATTERY"}
  // Unsupported types:
  // 0xFC  POCKET CAMERA
  // 0xFD  BANDAI TAMA5
  // 0xFE  HuC3
  // 0xFF  HuC1+RAM+BATTERY
};

const std::map<uint8_t, std::string> kRomSizes = {
  {0x00, "32KByte (no ROM banking)"},
  {0x01, "64KByte (4 banks)"},
  {0x02, "128KByte (8 banks)"},
  {0x03, "256KByte (16 banks)"},
  {0x04, "512KByte (32 banks)"},
  {0x05, "1MByte (64 banks)  - only 63 banks used by MBC1"},
  {0x06, "2MByte (128 banks) - only 125 banks used by MBC1"},
  {0x07, "4MByte (256 banks)"},
  {0x52, "1.1MByte (72 banks)"},
  {0x53, "1.2MByte (80 banks)"},
  {0x54, "1.5MByte (96 banks)"}
};

const std::map<uint8_t, std::string> kRamSizes = {
  {0x00, "None"},
  {0x01, "2 KBytes"},
  {0x02, "8 Kbytes"},
  {0x03, "32 KBytes (4 banks of 8KBytes each)"}
};

class MBCNone : public MemoryBankController {
 public:
  MBCNone(std::shared_ptr<MMapFile> rom) : MemoryBankController(rom) {}

  uint8_t Read8(uint16_t offset) override {
    if (offset > 0x7FFF) {
      FATALF("MBCNone: unsupported read offset: 0x%04x", offset);
    }
    return *(reinterpret_cast<uint8_t*>(rom_->memory()) + offset);
  }

  void Write8(uint16_t offset, uint8_t value) override {
    if (0x2000 <= offset && offset <= 0x3FFF) {
      if (value > 1) {
        FATALF("MBCNone does not support ROM bank 0x%02x", value & 0xff);
      }
      return;
    }
    FATALF("MBCNone: Unimplemented Write to Cartridge: (0x%04x) <- 0x%02x",
        offset & 0xffff, value & 0xff);
  }
};

class MBC1 : public MemoryBankController {
 public:
  MBC1(std::shared_ptr<MMapFile> rom) : MemoryBankController(rom) {}

  uint8_t Read8(uint16_t offset) override {
    if (offset > 0x7FFF) {
      FATALF("TODO: MBC1 unsupported read offset: 0x%04x", offset);
    }
    if (0x4000 <= offset && offset < 0x8000) {
      return *(reinterpret_cast<uint8_t*>(rom_->memory())
          + (0x4000 * rom_bank_) + (offset - 0x4000));
    }
    return *(reinterpret_cast<uint8_t*>(rom_->memory()) + offset);
  }

  void Write8(uint16_t offset, uint8_t value) override {
    if (0x2000 <= offset && offset < 0x4000) {
      // Lower 5 bits of ROM bank.
      rom_bank_ = (rom_bank_ & 0xe0) | (value & 0x1f);
      if (rom_bank_ == 0 || rom_bank_ == 20 ||
          rom_bank_ == 40 || rom_bank_ == 60) {
        rom_bank_ += 1;
      }
      return;
    }
    if (0x4000 <= offset && offset < 0x6000) {
      // RAM bank, or upper 2 bits of ROM bank.
      rom_bank_ = (rom_bank_ & 0x1f) | (value & 0xe0);
      if (rom_bank_ == 0 || rom_bank_ == 20 ||
          rom_bank_ == 40 || rom_bank_ == 60) {
        rom_bank_ += 1;
      }
      return;
    }
    if (0xA000 <= offset && offset < 0xC000) {
      // TODO: RAM
      ERRORF("TODO: MBC1: RAM Write to Cartridge: (0x%04x) <- 0x%02x",
          offset & 0xffff, value & 0xff);
      return;
    }
    FATALF("MBC1: Unimplemented Write to Cartridge: (0x%04x) <- 0x%02x",
        offset & 0xffff, value & 0xff);
  }

 private:
  int rom_bank_ = 1;
};

class MBC3 : public MemoryBankController {
 public:
  MBC3(std::shared_ptr<MMapFile> rom) : MemoryBankController(rom) {}

  uint8_t Read8(uint16_t offset) override {
    if (offset < 0x4000) {
      return *(reinterpret_cast<uint8_t*>(rom_->memory()) + offset);
    }
    if (0x4000 <= offset && offset < 0x8000) {
      uint32_t bank_offset = (0x4000 * rom_bank_) + (offset - 0x4000);
      if (bank_offset >= rom_->size()) {
        FATALF("Attempted to read past the end of the ROM file");
      }
      return *(reinterpret_cast<uint8_t*>(rom_->memory()) + bank_offset);
    }
    if (0xA000 <= offset && offset < 0xC000) {
      uint32_t bank_offset = (0x2000 * ram_bank_) + (offset - 0xA000);
      return ram_[bank_offset];
    }

    FATALF("TODO: MBC3 unimplemented read offset: 0x%04x", offset);
    return 0;
  }

  void Write8(uint16_t offset, uint8_t value) override {
    if (offset < 0x2000) {
      // TODO: support enable/disable of RAM & RTC so we know when to save to
      // disk
      return;
    }

    if (0x2000 <= offset && offset < 0x4000) {
      // All 7 bits of rom bank.
      rom_bank_ = value & 0x7f;
      if (rom_bank_ == 0) rom_bank_ = 1;
      return;
    }
    if (0x4000 <= offset && offset < 0x6000) {
      if (value > 0x3) { FATALF("TODO: implement MBC3 RTC registers"); }

      // RAM bank number 0x00-0x03.
      ram_bank_ = (value & 0x3);
      return;
    }
    if (0x6000 <= offset && offset < 0x8000) {
      // TODO: This is a latch clock data register. I have no idea what this
      // actually does.
      ERRORF("TODO: MBC3 latch clock data");
      return;
    }
    if (0xA000 <= offset && offset < 0xC000) {
      uint32_t bank_offset = (0x2000 * ram_bank_) + (offset - 0xA000);
      ram_[bank_offset] = value;
      return;
    }
    FATALF("MBC3: Unimplemented Write to Cartridge: (0x%04x) <- 0x%02x",
        offset & 0xffff, value & 0xff);
  }

  bool LoadRAM(const std::string &filename) override {
    if (!file::Exists(filename)) {
      INFOF("No save file found.");
      std::memset(ram_, 0, kRAMSize);  // Zero out the RAM block.
      return true;
    }

    std::vector<char> contents;
    bool result = file::ReadBytes(filename, &contents);
    if (!result) return false;
    INFOF("Read %lu bytes from file '%s'", contents.size(), filename.c_str());
    if (contents.size() != kRAMSize) {
      FATALF("Save file '%s' was not the correct size.", filename.c_str());
    }
    std::memcpy(ram_, contents.data(), kRAMSize);
    return true;
  }

  // TODO: mmap?
  bool PersistRAM(const std::string &filename) override {
    bool result = file::WriteBytes(filename, ram_, kRAMSize);
    if (!result) return false;
    INFOF("Wrote %d bytes to file '%s'", kRAMSize, filename.c_str());
    return true;
  }

 private:
  const static int kRAMSize = 0x8000;

  int rom_bank_ = 0;
  int ram_bank_ = 0;

  // 32kb of RAM.
  uint8_t ram_[kRAMSize];

  // Real-Time clock.
  MBC3RTC rtc_;
};

}  // namespace

void MBC3RTC::UpdateRTC() {
  // TODO: check if latched or halted
  uint64_t current_timestamp = CurrentSeconds();

  // Compue the seconds elapsed since base_timestamp_;
  uint64_t timestamp_delta = current_timestamp = base_timestamp_;

  base_rtc_ = ComputeNewRTCData(base_rtc_, timestamp_delta);
  base_timestamp_ = current_timestamp;
}

uint64_t MBC3RTC::RTCDataToTimestamp(const RTCData &rtc) {
  uint64_t timestamp = 0;
  timestamp += rtc.seconds;
  timestamp += rtc.minutes * 60;
  timestamp += rtc.hours * 3600;
  timestamp += rtc.days * 86400;
  return timestamp;
}

MBC3RTC::RTCData MBC3RTC::TimestampToRTCData(uint64_t seconds) {
  MBC3RTC::RTCData rtc;
  rtc.seconds = seconds % 60;
  rtc.minutes = (seconds / 60) % 60;
  rtc.hours = (seconds / 3600) % 24;
  unsigned days = seconds / 86400;
  rtc.days = days & 0x1FF;
  if (days > 0x1FF) rtc.days |= RTC_DAY_OVERFLOW_BIT;
  return rtc;
}

MBC3RTC::RTCData MBC3RTC::ComputeNewRTCData(const RTCData &base_rtc,
    uint64_t timestamp_delta) {

  // Convert the base RTC data into a Unix time delta.
  uint64_t base_seconds = RTCDataToTimestamp(base_rtc);

  // Add the actual elapsed time.
  uint64_t new_seconds = base_seconds + timestamp_delta;

  // Convert that Unix delta back into an RTCData struct (should account for
  // all overflow).
  return TimestampToRTCData(new_seconds);
}


Cartridge::Cartridge(
    const std::string &filename, const std::string &save_filename,
    std::shared_ptr<Clock> clock)
  : save_filename_(save_filename), rom_(new MMapFile(filename)) {
  uint8_t type_byte = rom_->Read8(kCartridgeTypeAddress);
  switch (type_byte) {
    case 0x0:
      mbc_.reset(new MBCNone(rom_));
      break;
    case 0x1:
      mbc_.reset(new MBC1(rom_));
      break;
    case 0x13:
      mbc_.reset(new MBC3(rom_));
      break;
    default:
      FATALF("Unsupported cartridge type: 0x%02x", type_byte & 0xff);
  }

  if (!mbc_->LoadRAM(save_filename_)) {
    FATALF("Failed to load save file '%s'.", save_filename_.c_str());
  }

  clock->RegisterObserver([this](int cycles) {
    cycle_accumulator_ += cycles;
    if (cycle_accumulator_ > kWriteCheckInterval) {
      MaybeWrite();
      cycle_accumulator_ = 0;
    }
  });
}

uint8_t Cartridge::Read8(uint16_t offset) {
  return mbc_->Read8(offset);

  if (0x3FFF < offset && offset < 0x8000 && rom_bank_ > 1) {
    FATALF("NOT IMPLEMENTED: rom banking");
  }
  return *(reinterpret_cast<uint8_t*>(rom_->memory()) + offset);
}

void Cartridge::Write8(uint16_t offset, uint8_t value) {
  mbc_->Write8(offset, value);
  // Ensure it's a ram write.
  // TODO: make sure this works for all MBC types.
  if (0xA000 <= offset && offset <= 0xBFFF) {
    Dirty();
  }
  return;
}

bool Cartridge::Persist() {
  return mbc_->PersistRAM(save_filename_);
}

std::string Cartridge::Title() {
  char *title = reinterpret_cast<char*>(rom_->memory()) + 0x134;
  char buf[17];
  std::strncpy(buf, title, 16);
  return std::string(buf);
}

void Cartridge::PrintCartridgeDebug() {
  std::string title = Title();
  INFOF("Cartridge: %s", title.c_str());

  uint8_t type_byte = Read8(kCartridgeTypeAddress);
  auto type = kCartridgeTypes.find(type_byte);
  if (type == kCartridgeTypes.end()) {
    INFOF("Unknown Cartridge Type: 0x%01x", type_byte & 0xff);
  } else {
    INFOF("Cartridge Type (0x%01x): %s",
        type_byte & 0xff, type->second.c_str());
  }

  uint8_t rom_size_byte = Read8(0x148);
  auto rom_size = kRomSizes.find(rom_size_byte);
  if (rom_size == kRomSizes.end()) {
    INFOF("Unrecognized ROM Size: 0x%01x", rom_size_byte & 0xff);
  } else {
    INFOF("ROM Size (0x%01x): %s",
        rom_size_byte & 0xff, rom_size->second.c_str());
  }

  uint8_t ram_size_byte = Read8(0x149);
  auto ram_size = kRamSizes.find(ram_size_byte);
  if (ram_size == kRamSizes.end()) {
    INFOF("Unrecognized RAM Size: 0x%01x", ram_size_byte & 0xff);
  } else {
    INFOF("RAM Size (0x%01x): %s",
        ram_size_byte & 0xff, ram_size->second.c_str());
  }
}

void Cartridge::Dirty() {
  dirty_ = true;
  last_dirty_timestamp_ = CurrentNanos();
}

void Cartridge::MaybeWrite() {
  if (!dirty_ || CurrentNanos() < last_dirty_timestamp_ + kWriteDelay) return;
  bool result = Persist();
  if (!result) {
    ERRORF("Failed to write to file '%s'.", save_filename_.c_str());
    return;
  }
  dirty_ = false;
}

}  // namespace dgb
