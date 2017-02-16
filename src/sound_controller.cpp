#include "sound_controller.h"

#include "common/logging.h"

namespace dgb {

void SoundController::Write8(uint16_t address, uint8_t value) {
  FATALF("TODO: write to sound controller: 0x%04x <- 0x%02x", address, value);
}

uint8_t SoundController::Read8(uint16_t address) {
  if (address == 0xff11) { return channel1_pattern_; }

  FATALF("TODO: read from sound controller: 0x%04x", address);
  return 0;
}

}  // namespace dgb
