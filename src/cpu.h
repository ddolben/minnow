#ifndef DGB_CPU_H_
#define DGB_CPU_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "clock.h"
#include "logging.h"
#include "memory.h"


namespace dgb {

class CPU {
 public:
  CPU(std::shared_ptr<Clock> clock) : clock_(clock) {}

  bool ProcessInterrupts(Memory *memory);

  bool RunOp(Memory *memory, int *cycle_count);
  bool RunPrefix(uint8_t code, Memory *memory);

  void PrintRegisters() {
    DEBUGF("  PC: 0x%04x", 0xffff & pc_);
    DEBUGF("  SP: 0x%04x", 0xffff & sp_);
    DEBUGF("  AF: 0x%04x", 0xffff & af_);
    DEBUGF("  BC: 0x%04x", 0xffff & bc_);
    DEBUGF("  DE: 0x%04x", 0xffff & de_);
    DEBUGF("  HL: 0x%04x", 0xffff & hl_);
  }

  void Wait() {
    thread_.join();
  }

  void Kill() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
  }

  bool IsRunning() {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_running_;
  }

  void Loop(Memory *memory) {
    int cycle_count;
    while (IsRunning()) {
      if (!RunOp(memory, &cycle_count)) {
        break;
      }
      if (ime_) {
        if (!ProcessInterrupts(memory)) {
          break;
        }
      }

      clock_->Tick(cycle_count);
    }
  }

  // Starts the CPU loop in a separate thread.
  void StartLoop(Memory *memory) {
    thread_ = std::thread([this, memory]{
      this->Loop(memory);
    });
  }

  void set_pc(uint16_t value) { pc_ = value; }
  uint8_t pc() { return pc_; }

  void set_breakpoint(uint16_t value) { breakpoint_ = value; }

 private:
  enum FlagsMask {
    ZERO_FLAG = 0x80,
    SUBTRACT_FLAG = 0x40,
    HALF_CARRY_FLAG = 0x20,
    CARRY_FLAG = 0x10
  };

  const uint16_t kInterruptHandlers[5] = {
    0x40, 0x48, 0x50, 0x58, 0x60
  };

  std::shared_ptr<Clock> clock_;

  std::thread thread_;
  std::mutex mutex_;
  bool is_running_ = true;
  bool debug_ = false;
  int64_t breakpoint_ = -1;

  // Op helpers.
  uint8_t LoadData8(uint8_t *dest, Memory *memory);
  uint16_t LoadData16(uint16_t *dest, Memory *memory);
  uint8_t LoadData8ToMem(uint16_t dest_addr, Memory *memory);
  void LoadMem8(uint8_t *dest, uint16_t addr, Memory *memory);
  void LoadReg8(uint8_t *dest, uint8_t value);
  void Inc8(uint8_t *value);
  void Inc16(uint16_t *value);
  void Dec8(uint8_t *value);
  void Dec16(uint16_t *value);
  void Add8(uint8_t *dest, uint8_t value);
  void Add16(uint16_t *dest, uint16_t value);
  void Sub8(uint8_t *dest, uint8_t value);
  void And(uint8_t value);
  void Or(uint8_t value);
  void Xor(uint8_t value);
  void Push(uint16_t value, Memory *memory);
  void Pop(uint16_t *dest, Memory *memory);
  void Jump(bool do_jump, Memory *memory);
  uint8_t JumpRelative(bool do_jump, Memory *memory);
  void Return(bool do_return, Memory *memory);
  void RotateLeftThroughCarry(uint8_t *value);
  void Swap (uint8_t *dest, uint8_t value);
  void TestBit(uint8_t value, unsigned int bit_index);
  void SetBit(uint8_t *dest, unsigned int bit_index);
  void ResetBit(uint8_t *dest, unsigned int bit_index);

  // Program counter.
  uint16_t pc_ = 0x0;
  // Stack pointer.
  uint16_t sp_ = 0x0;

  uint16_t af_ = 0x0;
  uint8_t *a_ = reinterpret_cast<uint8_t*>(&af_) + 1;
  // Flags register.
  //   Bit:  7 6 5 4 3 2 1 0
  //   Flag: Z N H C 0 0 0 0
  uint8_t *f_ = reinterpret_cast<uint8_t*>(&af_);

  uint16_t bc_ = 0x0;
  uint8_t *b_ = reinterpret_cast<uint8_t*>(&bc_) + 1;
  uint8_t *c_ = reinterpret_cast<uint8_t*>(&bc_);

  uint16_t de_ = 0x0;
  uint8_t *d_ = reinterpret_cast<uint8_t*>(&de_) + 1;
  uint8_t *e_ = reinterpret_cast<uint8_t*>(&de_);

  uint16_t hl_ = 0x0;
  uint8_t *h_ = reinterpret_cast<uint8_t*>(&hl_) + 1;
  uint8_t *l_ = reinterpret_cast<uint8_t*>(&hl_);

  // Interrupt Master Enable flag.
  // TODO: default to false?
  bool ime_ = false;
};

}  // namespace dgb

#endif  // DGB_CPU_H_
