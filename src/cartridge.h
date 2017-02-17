#ifndef DGB_CARTRIDGE_H_
#define DGB_CARTRIDGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "clock.h"
#include "mmapfile.h"


namespace dgb {

// Emulates the cartridge MBC.
class MemoryBankController {
 public:
  MemoryBankController(std::shared_ptr<MMapFile> rom) : rom_(rom) {}

  virtual uint8_t Read8(uint16_t offset) = 0;
  virtual void Write8(uint16_t offset, uint8_t value) = 0;
  virtual bool LoadRAM(const std::string &filename) {
    INFOF("NOT IMPLEMENTED: MBC file read");
    return true;
  }
  virtual bool PersistRAM(const std::string &filename) {
    INFOF("NOT IMPLEMENTED: MBC file write");
    return true;
  }

 protected:
  std::shared_ptr<MMapFile> rom_;
};

class Cartridge {
 public:
  Cartridge(const std::string &filename, const std::string &save_filename,
            std::shared_ptr<Clock> clock);

  uint8_t Read8(uint16_t offset);
  void Write8(uint16_t offset, uint8_t value);

  bool Persist();

  std::string Title();
  void PrintCartridgeDebug();

 private:
  // The time after which a change to the RAM is written to disk. Measured
  // after the last change so as to group subsequent changes into a single
  // write.
  const static uint64_t kWriteDelay = 5e9;  // 5 seconds.

  // The number of cycles after which to check if we need to write the save
  // file.
  const static int kWriteCheckInterval = 4000000;  // ~1 second

  // Makes it dirty;
  void Dirty();
  // Writes to the file, if it's been long enough.
  void MaybeWrite();

  bool dirty_ = false;
  uint64_t last_dirty_timestamp_ = 0;
  std::string save_filename_;
  std::shared_ptr<MMapFile> rom_;
  std::unique_ptr<MemoryBankController> mbc_;
  uint8_t rom_bank_ = 0;
  uint64_t cycle_accumulator_ = 0;
};

}  // namespace dgb

#endif  // DGB_CARTRIDGE_H_
