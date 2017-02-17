#ifndef DGB_CPU_H_
#define DGB_CPU_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "clock.h"
#include "interrupts.h"
#include "common/logging.h"
#include "memory.h"


namespace dgb {

const uint16_t kInterruptRequestAddress = 0xFF0F;
const uint16_t kInterruptEnableAddress = 0xFFFF;

const uint16_t kInterruptHandlers[5] = {
  0x40, 0x48, 0x50, 0x58, 0x60
};

class CPU {
 public:
  CPU(std::shared_ptr<Clock> clock, std::shared_ptr<Interrupts> interrupts);

  uint8_t Read8(uint16_t address, Memory *memory);
  uint16_t Read16(uint16_t address, Memory *memory);
  void Write8(uint16_t address, uint8_t value, Memory *memory);
  void Write16(uint16_t address, uint16_t value, Memory *memory);

  bool ProcessInterrupts(Memory *memory);

  void PrintRegisters();
  bool RunOp(Memory *memory, int *cycle_count);
  bool RunPrefix(uint8_t code, Memory *memory);

  void Wait() { thread_.join(); }
  void Kill() { is_running_.store(false); }
  bool IsRunning() { return is_running_.load(); }

  // Starts the CPU loop in a separate thread.
  // TODO: make memory an injected instance variable
  void StartLoop(Memory *memory);
  // Runs the CPU loop on the calling thread.
  void Loop(Memory *memory);

  void set_pc(uint16_t value) { pc_ = value; }
  uint8_t pc() { return pc_; }

  void set_breakpoint(uint16_t value) { breakpoint_ = value; }
  void set_breakpoint_opcode(int16_t value) { breakpoint_opcode_ = value; }

  void set_paused(bool value) { paused_.store(value); }
  bool paused() { return paused_.load(); }

  void set_debug(bool value) { debug_.store(value); }
  bool debug() { return debug_.load(); }

 private:
  enum FlagsMask {
    ZERO_FLAG = 0x80,
    SUBTRACT_FLAG = 0x40,
    HALF_CARRY_FLAG = 0x20,
    CARRY_FLAG = 0x10
  };

  // Op helpers.
  // TODO: make these all self-contained (not dependent on instance variables)
  // so they're easier to test.
  uint8_t LoadData8(uint8_t *dest, Memory *memory);
  uint16_t LoadData16(uint16_t *dest, Memory *memory);
  uint8_t LoadData8ToMem(uint16_t dest_addr, Memory *memory);
  void LoadReg8(uint8_t *dest, uint8_t value);
  void Inc8(uint8_t *value);
  void Inc16(uint16_t *value);
  void Dec8(uint8_t *value);
  void Dec16(uint16_t *value);
  void Add8(uint8_t *dest, uint8_t value);  // ADD
  // Performs an 8-bit ADD, but allows the arguments to be 16-bit numbers to
  // properly account for operands greater than 0xFF, for example to implement
  // ADC.
  uint8_t Add8With16(uint16_t a, uint16_t b);
  void AddCarry8(uint8_t *dest, uint8_t value);  // ADC
  void Add16(uint16_t *dest, uint16_t value);
  void Sub8(uint8_t *dest, uint8_t value);  // SUB
  // Performs an 8-bit SUB, but allows the arguments to be 16-bit numbers to
  // properly account for operands greater than 0xFF, for example to implement
  // ADC.
  uint8_t Sub8With16(uint16_t a, uint16_t b);
  void SubCarry8(uint8_t *dest, uint8_t value);  // SBC
  void DecimalAdjust(uint8_t *dest);  // DAA
  void Cp(uint8_t value);
  void And(uint8_t value);
  void Or(uint8_t value);
  void Xor(uint8_t value);
  void Push(uint16_t value, Memory *memory);
  void Pop(uint16_t *dest, Memory *memory);
  void Jump(bool do_jump, Memory *memory);
  uint8_t JumpRelative(bool do_jump, Memory *memory);
  void CallA16(bool do_call, Memory *memory);
  void Return(Memory *memory);

  void CCF();
  void Halt() { halted_ = true; }

  uint8_t RotateLeft(uint8_t value);  // RLC
  uint8_t RotateLeftThroughCarry(uint8_t value);  // RL
  uint8_t RotateRight(uint8_t value);  // RRC
  uint8_t RotateRightThroughCarry(uint8_t value);  // RR
  uint8_t ShiftLeft(uint8_t value);  // SLA
  uint8_t ShiftRight(uint8_t value);  // SRL
  uint8_t ShiftRightIntoCarry(uint8_t value);  // SRA
  uint8_t Swap(uint8_t value);
  void TestBit(uint8_t value, unsigned int bit_index);
  uint8_t SetBit(uint8_t value, unsigned int bit_index);
  uint8_t ResetBit(uint8_t value, unsigned int bit_index);

  std::thread thread_;
  std::atomic<bool> is_running_{true};
  std::atomic<bool> paused_{false};
  std::atomic<bool> debug_{false};
  bool halted_ = false;
  // Memory address at which to break.
  int64_t breakpoint_ = -1;
  // Opcode at which to break (it's actually just a uint8, but expanding to
  // 16-bit signed allows for negative numbers to disable it).
  int16_t breakpoint_opcode_ = -1;
  // Memory write address at which to break (actually uint16).
  int32_t breakpoint_write_min_ = -1;
  int32_t breakpoint_write_max_ = -1;
  // Memory read address at which to break (actually uint16).
  int32_t breakpoint_read_min_ = -1;
  int32_t breakpoint_read_max_ = -1;

  uint16_t previous_pc_ = 0;
  std::string previous_debug_command_;

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

  // Interrupt registers.
  uint8_t interrupt_request_ = 0;  // 0xFF0F
  uint8_t interrupt_enable_ = 0;  // 0xFFFF

  std::shared_ptr<Clock> clock_;
  std::shared_ptr<Interrupts> interrupts_;
};

}  // namespace dgb

#endif  // DGB_CPU_H_
