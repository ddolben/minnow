#ifndef DGB_MMAPFILE_H_
#define DGB_MMAPFILE_H_

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/logging.h"


namespace dgb {

class MMapFile {
 public:
  MMapFile(const std::string &filename) {
    fd_ = open(filename.c_str(), 0);
    CHECK(fd_ >= 0);

    size_ = lseek(fd_, 0, SEEK_END);

    memory_ = mmap(0, size_, PROT_READ, MAP_SHARED, fd_, 0);
    if (memory_ == MAP_FAILED) {
      perror("mmap");
    }
  }
  ~MMapFile() {
    CHECK(munmap(memory_, size_) == 0);
    CHECK(close(fd_) == 0);
  }

  uint8_t Read8(uint64_t offset) {
    CHECK(offset < size_);
    return *(reinterpret_cast<uint8_t*>(memory_) + offset);
  }

  void *memory() { return memory_; }

 private:
  int fd_ = -1;
  off_t size_ = 0;
  void *memory_ = nullptr;
};

}  // namespace dgb

#endif  // DGB_MMAPFILE_H_
