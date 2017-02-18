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

// CurrentNanos returns the current Unix Epoch timestamp in nanoseconds. The
// actual resolution of the timestamp depents on the system's implementation of
// std::chrono::high_resolution_clock.
inline uint64_t CurrentNanos() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// CurrentSeconds returns the current Unix Epoch timestamp in seconds. The
// actual resolution of the timestamp depents on the system's implementation of
// std::chrono::high_resolution_clock.
inline uint64_t CurrentSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

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
