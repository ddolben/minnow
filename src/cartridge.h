#ifndef DGB_CARTRIDGE_H_
#define DGB_CARTRIDGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "mmapfile.h"


namespace dgb {

class Cartridge {
 public:
  Cartridge(const std::string &filename);

  uint8_t Read8(uint16_t offset);
  uint16_t Read16(uint16_t offset);
  void Write8(uint16_t offset, uint8_t value);
  void Write16(uint16_t offset, uint16_t value);

  void PrintCartridgeDebug();

 private:
  std::unique_ptr<MMapFile> rom_;
  uint8_t rom_bank_ = 0;
};

}  // namespace dgb

#endif  // DGB_CARTRIDGE_H_
