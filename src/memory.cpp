#include "memory.h"

#include <cstdint>

namespace dgb {

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
