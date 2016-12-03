#ifndef DGB_CARTRIDGE_H_
#define DGB_CARTRIDGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "mmapfile.h"


namespace dgb {

// Emulates the cartridge MBC.
class MemoryBankController {
 public:
  MemoryBankController(std::shared_ptr<MMapFile> rom) : rom_(rom) {}

  virtual uint8_t Read8(uint16_t offset) = 0;
  virtual void Write8(uint16_t offset, uint8_t value) = 0;

 protected:
  std::shared_ptr<MMapFile> rom_;
};

class Cartridge {
 public:
  Cartridge(const std::string &filename);

  uint8_t Read8(uint16_t offset);
  void Write8(uint16_t offset, uint8_t value);

  std::string Title();
  void PrintCartridgeDebug();

 private:
  std::shared_ptr<MMapFile> rom_;
  std::unique_ptr<MemoryBankController> mbc_;
  uint8_t rom_bank_ = 0;
};

}  // namespace dgb

#endif  // DGB_CARTRIDGE_H_
