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
#include "common/logging.h"
#include "mmapfile.h"
#include "sound_controller.h"
#include "timers.h"


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
  Memory(std::shared_ptr<Cartridge> cartridge, std::shared_ptr<Display> display,
      std::shared_ptr<Input> input, std::shared_ptr<Timers> timers,
      std::shared_ptr<SoundController> sound_controller)
    : cartridge_(cartridge), display_(display), input_(input), timers_(timers),
      sound_controller_(sound_controller) {}

  void LoadBootloader(const std::string &filename);

  uint8_t Read8(uint16_t offset);
  void Write8(uint16_t offset, uint8_t value);

  bool bootstrap_is_mapped() { return bootstrap_is_mapped_; }

 private:
  uint8_t *GetOffset(uint16_t offset);
  void Write(uint16_t offset, uint8_t *value, int length);

  uint8_t ReadFromDevice(uint16_t offset);
  void WriteToDevice(uint16_t offset, uint8_t value);

  void StartDMATransfer(uint8_t value);

  bool bootstrap_is_mapped_ = false;
  std::unique_ptr<MMapFile> bootstrap_;
  uint8_t wram_[8192];  // 8k of working ram
  uint8_t high_ram_[128];  // 128 bytes of high ram

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
  std::shared_ptr<Timers> timers_;
  std::shared_ptr<SoundController> sound_controller_;
};

}  // namespace dgb

#endif  // DGB_MEMORY_H_
