#include "cartridge.h"

#include <map>
#include <string>

#include "logging.h"

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
    return *(reinterpret_cast<uint8_t*>(rom_->memory()) + offset);
  }

  void Write8(uint16_t offset, uint8_t value) override {
    // TODO: possibly RAM?
    WARNINGF("Unimplemented Write to Cartridge: (0x%04x) <- 0x%02x",
        offset & 0xffff, value & 0xff);
  }
};
}  // namespace

Cartridge::Cartridge(const std::string &filename)
    : rom_(new MMapFile(filename)) {
  uint8_t type_byte = rom_->Read8(kCartridgeTypeAddress);
  if (type_byte != 0) {
    FATALF("Unsupported cartridge type: 0x%02x", type_byte & 0xff);
  }
  mbc_.reset(new MBCNone(rom_));
}

uint8_t Cartridge::Read8(uint16_t offset) {
  return mbc_->Read8(offset);

  if (0x3FFF < offset && offset < 0x8000 && rom_bank_ > 1) {
    FATALF("NOT IMPLEMENTED: rom banking");
  }
  return *(reinterpret_cast<uint8_t*>(rom_->memory()) + offset);
}

void Cartridge::Write8(uint16_t offset, uint8_t value) {
  //if (offset < 0x8000) {
  //  ERRORF("Unimplemented Write to Cartridge RAM: (0x%04x) <- 0x%02x",
  //      offset & 0xffff, value & 0xff);
  //  return;
  //}
  //if (0xA000 <= offset && offset <= 0xBFFF) {
  //  ERRORF("Unimplemented Write to Cartridge RAM: (0x%04x) <- 0x%02x",
  //      offset & 0xffff, value & 0xff);
  //  return;
  //}
  mbc_->Write8(offset, value);
  return;

  if (0x2000 <= offset && offset <= 0x3FFF) {
    // Lower 5 bits of ROM bank.
    rom_bank_ = value & 0x1f;
    if (rom_bank_ == 0) {
      rom_bank_ = 1;
    } else if (rom_bank_ > 1) {
      FATALF("Unsupported ROM bank: 0x%02x", value & 0xff);
    }
    return;
  }
  FATALF("Unimplemented Write to Cartridge: (0x%04x) <- 0x%02x",
      offset & 0xffff, value & 0xff);
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

}  // namespace dgb
