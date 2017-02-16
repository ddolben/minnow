#ifndef COMMON_TIME_TIME_H_
#define COMMON_TIME_TIME_H_

#include <chrono>
#include <cstdint>

namespace {

// Pause executes an assembly "pause" instruction. This should effectively be a
// nop instruction, but will instruct the assembler to make spin waits more
// efficient.
inline void Pause() {
  asm("pause;");
}

}  // namespace

// SpinWait performs a very accurate wait by spinning the CPU until the desired
// time has elapsed.
// Note that this may cause a large load on the CPU.
inline void SpinWait(uint64_t nanoseconds) {
  auto end = std::chrono::high_resolution_clock::now() +
      std::chrono::nanoseconds{nanoseconds};
  while (std::chrono::high_resolution_clock::now() < end) {
    Pause();
  }
}

#endif  // COMMON_TIME_TIME_H_
