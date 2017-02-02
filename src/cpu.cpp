#include "cpu.h"

#include <cstdint>
#include <functional>
#include <iostream>

#include "logging.h"
#include "memory.h"

namespace dgb {

inline uint8_t CPU::Read8(uint16_t address, Memory *memory) {
  if (address == breakpoint_read_) { debug_ = true; }
  CHECK(memory != nullptr);

  switch (address) {
    case kInterruptRequestAddress:
      return interrupt_request_;
    case kInterruptEnableAddress:
      return interrupt_enable_;
    default:
      return memory->Read8(address);
  }
}

inline uint16_t CPU::Read16(uint16_t address, Memory *memory) {
  if (address == breakpoint_read_) { debug_ = true; }
  CHECK(memory != nullptr);

  uint16_t value;
  uint8_t *ptr = reinterpret_cast<uint8_t*>(&value);
  ptr[0] = memory->Read8(address);
  ptr[1] = memory->Read8(address+1);
  return value;
}

inline void CPU::Write8(uint16_t address, uint8_t value, Memory *memory) {
  if (address == breakpoint_write_) { debug_ = true; }
  CHECK(memory != nullptr);

  switch (address) {
    case kInterruptRequestAddress:
      interrupt_request_ = value;
      break;
    case kInterruptEnableAddress:
      if ((value & (~(
           INTERRUPT_VBLANK |
           INTERRUPT_LCD_STAT |
           INTERRUPT_TIMER |
           INTERRUPT_SERIAL))) != 0)
        FATALF("Unimplemented interrupt enable: 0x%02x", value);
      interrupt_enable_ = value;
      break;
    default:
      memory->Write8(address, value);
  }
}

inline void CPU::Write16(uint16_t address, uint16_t value, Memory *memory) {
  if (address == breakpoint_write_) { debug_ = true; }
  CHECK(memory != nullptr);

  uint8_t *ptr = reinterpret_cast<uint8_t*>(&value);
  memory->Write8(address, ptr[0]);
  memory->Write8(address + 1, ptr[1]);
}

// Loads 8 bits of data.
inline uint8_t CPU::LoadData8(uint8_t *dest, Memory *memory) {
  uint8_t data = Read8(pc_, memory);
  pc_ += 1;
  *dest = data;
  return data;
}

// Loads 8 bits of data to a memory address.
inline uint8_t CPU::LoadData8ToMem(uint16_t dest_addr, Memory *memory) {
  uint8_t data = Read8(pc_, memory);
  pc_ += 1;
  Write8(dest_addr, data, memory);
  return data;
}

// Loads 16 bits of data.
inline uint16_t CPU::LoadData16(uint16_t *dest, Memory *memory) {
  uint16_t data = Read16(pc_, memory);
  pc_ += 2;
  *dest = data;
  return data;
}

// Increments 8-bit value by one and sets flags accordingly.
inline void CPU::Inc8(uint8_t *value) {
  // Check if the lower 4 bits are 1, which means the inc operation will
  // trigger a "half carry" from bit 3->4.
  uint8_t half_carry_bit = ((*value & 0x0f) == 0x0f) ? HALF_CARRY_FLAG : 0;

  // Increment the value.
  *value += 1;

  // Check if the new value is zero.
  uint8_t zero_bit = (*value == 0) ? ZERO_FLAG : 0;

  // Set flags appropriately.
  *f_ = zero_bit | half_carry_bit | (*f_ & CARRY_FLAG);
}

// Increments 16-bit value by one.
inline void CPU::Inc16(uint16_t *value) {
  *value += 1;
}

inline void CPU::Dec8(uint8_t *value) {
  // Check if the lower 4 bits are 0, which means the dec operation will
  // trigger a "half borrow" from bit 4->3.
  uint8_t half_carry_bit = ((*value & 0x0f) == 0x00) ? HALF_CARRY_FLAG : 0;

  // Decrement the value.
  *value -= 1;

  // Check if the new value is zero.
  uint8_t zero_bit = (*value == 0) ? ZERO_FLAG : 0;

  // Set flags appropriately.
  *f_ = zero_bit | SUBTRACT_FLAG | half_carry_bit | (*f_ & CARRY_FLAG);
}

inline void CPU::Dec16(uint16_t *value) {
  *value -= 1;
}

inline void CPU::Add8(uint8_t *dest, uint8_t value) {
  *f_ = 0x0;  // Reset subtraction flag.
  if ((value & 0xf) > (0xf - (*dest & 0xf))) *f_ |= HALF_CARRY_FLAG;
  if (value > (0xff - *dest)) *f_ |= CARRY_FLAG;

  *dest += value;
  if (*dest == 0) *f_ |= ZERO_FLAG;
}

inline void CPU::AddCarry8(uint8_t *dest, uint8_t value) {
  uint8_t carry = (*f_ & CARRY_FLAG) >> 4;
  if (value == 0xff && carry > 0) FATALF("TODO: fix the ADC value overflow");
  Add8(dest, value + carry);
}

inline void CPU::Add16(uint16_t *dest, uint16_t value) {
  *f_ = (*f_ & ZERO_FLAG);  // Reset subtraction flag, leave zero flag alone.
  if ((value & 0xfff) > (0xfff - (*dest & 0xfff))) *f_ |= HALF_CARRY_FLAG;
  if (value > (0xffff - *dest)) *f_ |= CARRY_FLAG;

  *dest += value;
}

inline void CPU::Sub8(uint8_t *dest, uint8_t value) {
  *f_ = SUBTRACT_FLAG;  // Set subtraction flag.
  if (*dest == value) *f_ |= ZERO_FLAG;
  if ((*dest & 0xf) < (value & 0xf)) *f_ |= HALF_CARRY_FLAG;
  if (*dest < value) *f_ |= CARRY_FLAG;

  *dest -= value;
}

inline void CPU::SubCarry8(uint8_t *dest, uint8_t value) {
  uint8_t carry = (*f_ & CARRY_FLAG) >> 4;
  if (value == 0xff && carry > 0) FATALF("TODO: fix the SBC value overflow");
  Sub8(dest, value + carry);
}

inline void CPU::DecimalAdjust(uint8_t *dest) {
  bool new_carry = false;

  if ((*f_ & SUBTRACT_FLAG) == 0) {  // Addition
    if ((*dest & 0xf) > 0x9 || ((*f_ & HALF_CARRY_FLAG) != 0)) {
      *dest += 6;
    }
    if ((*dest & 0xf0) > 0x90 || ((*f_ & CARRY_FLAG) != 0)) {
      *dest += 0x60;
      new_carry = true;
    }
  } else {  // Subtraction
    if ((*dest & 0xf) > 0x9 || ((*f_ & HALF_CARRY_FLAG) != 0)) {
      *dest -= 0x6;
    }
    if ((*dest & 0xf0) > 0x90 || ((*f_ & CARRY_FLAG) != 0)) {
      *dest -= 0x60;
    }

    // In subtraction, carry flag stays the same.
    new_carry = ((*f_ & CARRY_FLAG) != 0);
  }

  // TODO: check flag adjustments
  *f_ = (*f_ & SUBTRACT_FLAG);
  if (*dest == 0) *f_ |= ZERO_FLAG;
  if (new_carry) *f_ |= CARRY_FLAG;
}

// NOTE: this is a subtraction operation, but we throw away the result.
inline void CPU::Cp(uint8_t value) {
  *f_ = SUBTRACT_FLAG;  // Set subtraction flag.
  if (*a_ == value) *f_ |= ZERO_FLAG;
  if ((*a_ & 0xff) < (value & 0xff)) *f_ |= HALF_CARRY_FLAG;
  if (*a_ < value) *f_ |= CARRY_FLAG;
}

// Sets the A register to the result of A AND'ed with value.
inline void CPU::And(uint8_t value) {
  *a_ = *a_ & value;
  *f_ = (*a_ == 0) ? ZERO_FLAG : 0;
  *f_ |= HALF_CARRY_FLAG;
}

// Sets the A register to the result of A OR'd with value.
inline void CPU::Or(uint8_t value) {
  *a_ = *a_ | value;
  *f_ = (*a_ == 0) ? ZERO_FLAG : 0;
}

// Sets the A register to the result of A XOR'd with value.
inline void CPU::Xor(uint8_t value) {
  *a_ = *a_ ^ value;
  *f_ = (*a_ == 0) ? ZERO_FLAG : 0;
}

// Pushes a 16-bit value onto the stack.
inline void CPU::Push(uint16_t value, Memory *memory) {
  Write16(sp_ - 2, value, memory);
  sp_ -= 2;
}

// Pops a 16-bit value off the stack and into the given register.
inline void CPU::Pop(uint16_t *dest, Memory *memory) {
  *dest = Read16(sp_, memory);
  sp_ += 2;
}

inline void CPU::Jump(bool do_jump, Memory *memory) {
  uint16_t addr = Read16(pc_, memory);
  pc_ += 2;
  if (do_jump) {
    pc_ = addr;
  }
}

inline uint8_t CPU::JumpRelative(bool do_jump, Memory *memory) {
  // Use a signed byte here so we get subtraction too.
  int8_t jump = static_cast<int8_t>(Read8(pc_, memory));
  pc_ += 1;
  if (do_jump) {
    pc_ += jump;
  }
  return jump;
}

inline void CPU::CallA16(bool do_call, Memory *memory) {
  uint16_t addr = Read16(pc_, memory);
  pc_ += 2;
  if (do_call) {
    Write16(sp_ - 2, pc_, memory);
    sp_ -= 2;
    pc_ = addr;
  }
}

inline void CPU::Return(Memory *memory) {
  uint16_t addr = Read16(sp_, memory);
  sp_ += 2;
  pc_ = addr;
}

inline void CPU::CCF() {
  // Complement the carry bit.
  *f_ ^= CARRY_FLAG;
  // Reset the subtract and half-carry bits.
  *f_ &= ~(SUBTRACT_FLAG | HALF_CARRY_FLAG);
}

// Does a bitwise left rotation, putting the old bit 7 in the carry flag.
inline uint8_t CPU::RotateLeft(uint8_t value) {
  uint8_t left_bit = value & 0x80;
  // Shift left, filling rightmost bit with the old bit 7 value.
  uint8_t new_value = (value << 1) | (left_bit >> 7);
  // Set the carry flag to the old value's 8th bit, and set the zero flag if
  // the result was zero.
  uint8_t zero_bit = (new_value == 0) ? ZERO_FLAG : 0;
  *f_ = (left_bit >> 3) | zero_bit;

  return new_value;
}

// Does a bitwise left rotation, using the carry bit as a 9th bit in the
// operation.
inline uint8_t CPU::RotateLeftThroughCarry(uint8_t value) {
  uint8_t left_bit = value & 0x80;
  // Shift left, filling rightmost bit with carry flag's value.
  uint8_t new_value = (value << 1) | ((*f_ & CARRY_FLAG) >> 4);
  // Set the carry flag to the old value's 8th bit, and set the zero flag if
  // the result was zero.
  uint8_t zero_bit = (new_value == 0) ? ZERO_FLAG : 0;
  *f_ = (left_bit >> 3) | zero_bit;
  return new_value;
}

// Does a bitwise right rotation, putting the old bit 0 in the carry flag.
inline uint8_t CPU::RotateRight(uint8_t value) {
  uint8_t right_bit = value & 0x01;
  // Shift right, filling leftmost bit with the old bit 0 value.
  uint8_t new_value = (value >> 1) | (right_bit << 7);
  // Set the carry flag to the old value's 0th bit, and set the zero flag if
  // the result was zero.
  uint8_t zero_bit = (new_value == 0) ? ZERO_FLAG : 0;
  *f_ = (right_bit << 4) | zero_bit;

  return new_value;
}

inline uint8_t CPU::RotateRightThroughCarry(uint8_t value) {
  uint8_t right_bit = value & 0x01;
  // Shift right, filling leftmost bit with carry flag's value.
  uint8_t new_value = (value >> 1) | ((*f_ & CARRY_FLAG) >> 3);
  // Set the carry flag to the old value's 0th bit, and set the zero flag if
  // the result was zero.
  uint8_t zero_bit = (new_value == 0) ? ZERO_FLAG : 0;
  *f_ = (right_bit << 4) | zero_bit;
  return new_value;
}

bool CPU::ProcessInterrupts(Memory *memory) {
  // TODO: move these registers into the CPU class
  uint8_t interrupt_request = Read8(0xFF0F, memory);
  uint8_t interrupt_enable = Read8(0xFFFF, memory);
  uint8_t interrupts = interrupt_enable & interrupt_request;
  for (int i = 0; i < 5; ++i) {
    if ((interrupts & (1 << i)) > 0) {
      // Transfer execution to the requested interrupt handler.
      Push(pc_, memory);
      pc_ = kInterruptHandlers[i];
      ime_ = false;
      // Zero out the IR bit for the interrupt we're handling.
      Write8(0xFF0F, interrupt_request & (~(1 << i)), memory);
      // Set halt to false, so we resume CPU execution if halted.
      halted_ = false;
      // Break out of this loop and allow the CPU to execute the interrupt
      // handler. Since we've seroed out the IR bit for the interrupt we're
      // executing, as soon as RETI gets called, ProcessInterrupts will run
      // again and process interrupts of lower priority, if any.
      break;
    }
  }
  return true;
}

namespace {
// TODO: function pointers?
struct Op {
  uint8_t code;
  uint8_t length;
  uint8_t cycle_count;
  std::string debug;
};

// TODO: handle ops with different cycle counts depending on branch (e.g.
// jumps)
// TODO: make sure to handle CB prefix
// TODO: generate code for load ops?
Op ops[256] = {
#include "opcodes.inl"
};

// TODO: generate code for CB-prefixed opcodes
Op cb_ops[256] = {
#include "cb_opcodes.inl"
};
}  // namespace

bool CPU::RunOp(Memory *memory, int *cycle_count) {
  CHECK(memory != nullptr);
  CHECK(cycle_count != nullptr);

  // Consume the opcode and advance the program counter.
  uint8_t code = Read8(pc_, memory);

  // Check to see if any of the breakpoint conditions were met.
  if (pc_ == breakpoint_ || code == breakpoint_opcode_) {
    set_debug(true);
  }

  if (debug()) {
    PrintRegisters();
    DEBUGF("Previous PC: 0x%04x", previous_pc_);

    Op op = ops[code];
    if (op.length == 3) {
      DEBUGF("[0x%04x]: 0x%02x 0x%02x 0x%02x %s",
          pc_ & 0xffff, code & 0xff,
          Read8(pc_ + 1, memory) & 0xff,
          Read8(pc_ + 2, memory) & 0xff,
          op.debug.c_str());
    } else if (op.length == 2) {
      DEBUGF("[0x%04x]: 0x%02x 0x%02x     %s",
          pc_ & 0xffff, code & 0xff,
          Read8(pc_ + 1, memory) & 0xff,
          op.debug.c_str());
    } else if (code == 0xCB) {
      uint8_t cb_code = Read8(pc_ + 1, memory);
      Op cb_op = cb_ops[cb_code];
      DEBUGF("[0x%04x]: 0x%02x 0x%02x     %s",
          pc_ & 0xffff, code & 0xff, cb_code & 0xff, cb_op.debug.c_str());
    } else {
      DEBUGF("[0x%04x]: 0x%02x          %s",
          pc_ & 0xffff, code & 0xff, op.debug.c_str());
    }

    // TODO: formalize this as a debugger
    std::string line;
    while (true) {
      printf("debug> ");
      getline(std::cin, line);
      if (line.length() == 0) {
        printf("%s\n", previous_debug_command_.c_str());
        line = previous_debug_command_;
      } else {
        previous_debug_command_ = line;
      }

      if (std::string("continue").compare(line.substr(0, 8)) == 0) {
        set_debug(false);
        break;
      }
      if (std::string("print").compare(line.substr(0, 5)) == 0) {
        int addr = stoi(line.substr(6), 0, 16);
        if (addr > 0xffff) {
          WARNINGF("Cannot read memory at address 0x%x", addr);
        } else {
          uint8_t val = Read8(addr, memory);
          printf("[0x%04x]: 0x%02x\n", addr & 0xffff, val & 0xff);
        }
      } else if (std::string("break write").compare(line.substr(0, 11)) == 0) {
        breakpoint_write_ = stoi(line.substr(12), 0, 16);
        set_debug(false);
        break;
      } else if (std::string("break read").compare(line.substr(0, 10)) == 0) {
        breakpoint_read_ = stoi(line.substr(11), 0, 16);
        set_debug(false);
        break;
      } else if (std::string("break op").compare(line.substr(0, 8)) == 0) {
        breakpoint_opcode_ = stoi(line.substr(9), 0, 16);
        set_debug(false);
        break;
      } else if (std::string("break").compare(line.substr(0, 5)) == 0) {
        breakpoint_ = stoi(line.substr(6), 0, 16);
        set_debug(false);
        break;
      } else if (std::string("step").compare(line.substr(0, 4)) == 0) {
        break;
      } else if (std::string("quit").compare(line.substr(0, 4)) == 0) {
        exit(0);
      }
    }
  }

  previous_pc_ = pc_;
  pc_++;


  switch (code) {
  case 0x00:
    break;
  case 0x01:
    LoadData16(&bc_, memory);
    break;
  case 0x02: Write8(bc_, *a_, memory); break;
  case 0x03: Inc16(&bc_); break;
  case 0x04: Inc8(b_); break;
  case 0x05: Dec8(b_); break;
  case 0x06:
    LoadData8(b_, memory);
    break;
  case 0x07:
    *a_ = RotateLeft(*a_);
    break;
  case 0x09:
    Add16(&hl_, bc_);
    break;
  case 0x0a:
    *a_ = Read8(bc_, memory);
    break;
  case 0x0b: Dec16(&bc_); break;
  case 0x0c: Inc8(c_); break;
  case 0x0d: Dec8(c_); break;
  case 0x0e:
    LoadData8(c_, memory);
    break;
  case 0x0f: *a_ = RotateRight(*a_); break;
  case 0x11:
    LoadData16(&de_, memory);
    break;
  case 0x12:
    Write8(de_, *a_, memory);
    break;
  case 0x13: Inc16(&de_); break;
  case 0x14: Inc8(d_); break;
  case 0x15: Dec8(d_); break;
  case 0x16:
    LoadData8(d_, memory);
    break;
  case 0x17: *a_ = RotateLeftThroughCarry(*a_); break;
  case 0x18:
    JumpRelative(true, memory);
    break;
  case 0x19:
    Add16(&hl_, de_);
    break;
  case 0x1a:
    *a_ = Read8(de_, memory);
    break;
  case 0x1b: Dec16(&de_); break;
  case 0x1c: Inc8(e_); break;
  case 0x1d: Dec8(e_); break;
  case 0x1e:
    LoadData8(e_, memory);
    break;
  case 0x1f: *a_ = RotateRightThroughCarry(*a_); break;
  case 0x20: JumpRelative((*f_ & ZERO_FLAG) == 0, memory); break;
  case 0x21:
    LoadData16(&hl_, memory);
    break;
  case 0x22:
    Write8(hl_, *a_, memory);
    hl_ += 1;
    break;
  case 0x23: Inc16(&hl_); break;
  case 0x24: Inc8(h_); break;
  case 0x25: Dec8(h_); break;
  case 0x26:
    *h_ = Read8(pc_, memory);
    pc_++;
    break;
  case 0x27: DecimalAdjust(a_); break;
  case 0x28: JumpRelative((*f_ & ZERO_FLAG) != 0, memory); break;
  case 0x2a:
    *a_ = Read8(hl_, memory);
    hl_ += 1;
    break;
  case 0x2b: Dec16(&hl_); break;
  case 0x2c: Inc8(l_); break;
  case 0x2d: Dec8(l_); break;
  case 0x2e:
    LoadData8(l_, memory);
    break;
  case 0x2f:
    *a_ = ~(*a_);
    *f_ = SUBTRACT_FLAG | HALF_CARRY_FLAG;
    break;
  case 0x30: JumpRelative((*f_ & CARRY_FLAG) == 0, memory); break;
  case 0x31:
    LoadData16(&sp_, memory);
    break;
  case 0x32:
    Write8(hl_, *a_, memory);
    hl_ -= 1;
    break;
  case 0x33: Inc16(&sp_); break;
  case 0x34:
    {
      uint8_t value = Read8(hl_, memory);
      Inc8(&value);
      Write8(hl_, value, memory);
    }
    break;
  case 0x35:
    {
      uint8_t value = Read8(hl_, memory);
      Dec8(&value);
      Write8(hl_, value, memory);
    }
    break;
  case 0x36:
    LoadData8ToMem(hl_, memory);
    break;
  case 0x38: JumpRelative((*f_ & CARRY_FLAG) != 0, memory); break;
  case 0x3a:
    *a_ = Read8(hl_, memory);
    hl_--;
    break;
  case 0x3b: Dec16(&sp_); break;
  case 0x3c: Inc8(a_); break;
  case 0x3d: Dec8(a_); break;
  case 0x3e: LoadData8(a_, memory); break;
  case 0x3f: CCF(); break;

  case 0x40: *b_ = *b_; break;
  case 0x41: *b_ = *c_; break;
  case 0x42: *b_ = *d_; break;
  case 0x43: *b_ = *e_; break;
  case 0x44: *b_ = *h_; break;
  case 0x45: *b_ = *l_; break;
  case 0x46: *b_ = Read8(hl_, memory); break;
  case 0x47: *b_ = *a_; break;

  case 0x48: *c_ = *b_; break;
  case 0x49: *c_ = *c_; break;
  case 0x4a: *c_ = *d_; break;
  case 0x4b: *c_ = *e_; break;
  case 0x4c: *c_ = *h_; break;
  case 0x4d: *c_ = *l_; break;
  case 0x4e: *c_ = Read8(hl_, memory); break;
  case 0x4f: *c_ = *a_; break;

  case 0x50: *d_ = *b_; break;
  case 0x51: *d_ = *c_; break;
  case 0x52: *d_ = *d_; break;
  case 0x53: *d_ = *e_; break;
  case 0x54: *d_ = *h_; break;
  case 0x55: *d_ = *l_; break;
  case 0x56: *d_ = Read8(hl_, memory); break;
  case 0x57: *d_ = *a_; break;

  case 0x58: *e_ = *b_; break;
  case 0x59: *e_ = *c_; break;
  case 0x5a: *e_ = *d_; break;
  case 0x5b: *e_ = *e_; break;
  case 0x5c: *e_ = *h_; break;
  case 0x5d: *e_ = *l_; break;
  case 0x5e: *e_ = Read8(hl_, memory); break;
  case 0x5f: *e_ = *a_; break;

  case 0x60: *h_ = *b_; break;
  case 0x61: *h_ = *c_; break;
  case 0x62: *h_ = *d_; break;
  case 0x63: *h_ = *e_; break;
  case 0x64: *h_ = *h_; break;
  case 0x65: *h_ = *l_; break;
  case 0x66: *h_ = Read8(hl_, memory); break;
  case 0x67: *h_ = *a_; break;

  case 0x68: *l_ = *b_; break;
  case 0x69: *l_ = *c_; break;
  case 0x6a: *l_ = *d_; break;
  case 0x6b: *l_ = *e_; break;
  case 0x6c: *l_ = *h_; break;
  case 0x6d: *l_ = *l_; break;
  case 0x6e: *l_ = Read8(hl_, memory); break;
  case 0x6f: *l_ = *a_; break;

  case 0x70: Write8(hl_, *b_, memory); break;
  case 0x71: Write8(hl_, *c_, memory); break;
  case 0x72: Write8(hl_, *d_, memory); break;
  case 0x73: Write8(hl_, *e_, memory); break;
  case 0x74: Write8(hl_, *h_, memory); break;
  case 0x75: Write8(hl_, *l_, memory); break;
  case 0x76: Halt(); break;
  case 0x77: Write8(hl_, *a_, memory); break;

  case 0x78: *a_ = *b_; break;
  case 0x79: *a_ = *c_; break;
  case 0x7a: *a_ = *d_; break;
  case 0x7b: *a_ = *e_; break;
  case 0x7c: *a_ = *h_; break;
  case 0x7d: *a_ = *l_; break;
  case 0x7e: *a_ = Read8(hl_, memory); break;
  case 0x7f: *a_ = *a_; break;

  case 0x80: Add8(a_, *b_); break;
  case 0x81: Add8(a_, *c_); break;
  case 0x82: Add8(a_, *d_); break;
  case 0x83: Add8(a_, *e_); break;
  case 0x84: Add8(a_, *h_); break;
  case 0x85: Add8(a_, *l_); break;
  case 0x86: Add8(a_, Read8(hl_, memory)); break;
  case 0x87: Add8(a_, *a_); break;

  case 0x88: AddCarry8(a_, *b_); break;
  case 0x89: AddCarry8(a_, *c_); break;
  case 0x8a: AddCarry8(a_, *d_); break;
  case 0x8b: AddCarry8(a_, *e_); break;
  case 0x8c: AddCarry8(a_, *h_); break;
  case 0x8d: AddCarry8(a_, *l_); break;
  case 0x8e: AddCarry8(a_, Read8(hl_, memory)); break;
  case 0x8f: AddCarry8(a_, *a_); break;

  case 0x90: Sub8(a_, *b_); break;
  case 0x91: Sub8(a_, *c_); break;
  case 0x92: Sub8(a_, *d_); break;
  case 0x93: Sub8(a_, *e_); break;
  case 0x94: Sub8(a_, *h_); break;
  case 0x95: Sub8(a_, *l_); break;
  case 0x96: Sub8(a_, Read8(hl_, memory)); break;
  case 0x97: Sub8(a_, *a_); break;

  case 0x98: SubCarry8(a_, *b_); break;
  case 0x99: SubCarry8(a_, *c_); break;
  case 0x9a: SubCarry8(a_, *d_); break;
  case 0x9b: SubCarry8(a_, *e_); break;
  case 0x9c: SubCarry8(a_, *h_); break;
  case 0x9d: SubCarry8(a_, *l_); break;
  case 0x9e: SubCarry8(a_, Read8(hl_, memory)); break;
  case 0x9f: SubCarry8(a_, *a_); break;

  case 0xa0: And(*b_); break;
  case 0xa1: And(*c_); break;
  case 0xa2: And(*d_); break;
  case 0xa3: And(*e_); break;
  case 0xa4: And(*h_); break;
  case 0xa5: And(*l_); break;
  case 0xa6: And(Read8(hl_, memory)); break;
  case 0xa7: And(*a_); break;

  case 0xa8: Xor(*b_); break;
  case 0xa9: Xor(*c_); break;
  case 0xaa: Xor(*d_); break;
  case 0xab: Xor(*e_); break;
  case 0xac: Xor(*h_); break;
  case 0xad: Xor(*l_); break;
  case 0xae: Xor(Read8(hl_, memory)); break;
  case 0xaf: Xor(*a_); break;

  case 0xb0: Or(*b_); break;
  case 0xb1: Or(*c_); break;
  case 0xb2: Or(*d_); break;
  case 0xb3: Or(*e_); break;
  case 0xb4: Or(*h_); break;
  case 0xb5: Or(*l_); break;
  case 0xb6: Or(Read8(hl_, memory)); break;
  case 0xb7: Or(*a_); break;

  case 0xb8: Cp(*b_); break;
  case 0xb9: Cp(*c_); break;
  case 0xba: Cp(*d_); break;
  case 0xbb: Cp(*e_); break;
  case 0xbc: Cp(*h_); break;
  case 0xbd: Cp(*l_); break;
  case 0xbe: Cp(Read8(hl_, memory)); break;
  case 0xbf: Cp(*a_); break;

  case 0xc0:
    if ((*f_ & ZERO_FLAG) == 0) Return(memory);
    break;
  case 0xc1:
    Pop(&bc_, memory);
    break;
  case 0xc2:
    Jump((*f_ & ZERO_FLAG) == 0, memory);
    break;
  case 0xc3:
    {
      uint16_t addr = Read16(pc_, memory);
      pc_ = addr;
    }
    break;
  case 0xc4: CallA16((*f_ & ZERO_FLAG) == 0 , memory); break;
  case 0xc5:
    Push(bc_, memory);
    break;
  case 0xc6:
    Add8(a_, Read8(pc_, memory));
    pc_++;
    break;
  case 0xc8:
    if ((*f_ & ZERO_FLAG) > 0) Return(memory);
    break;
  case 0xc9:
    Return(memory);
    break;
  case 0xca:
    Jump((*f_ & ZERO_FLAG) > 0, memory);
    break;
  case 0xcb:
    {
      // Consume the code and advance the program counter.
      // TODO: cycle count
      uint8_t code = Read8(pc_, memory);
      pc_++;
      return RunPrefix(code, memory);
    }
    break;  // Unreachable.
  case 0xcc: CallA16((*f_ & ZERO_FLAG) != 0, memory); break;
  case 0xcd: CallA16(true, memory); break;
  case 0xd0: if ((*f_ & CARRY_FLAG) == 0) Return(memory); break;
  case 0xd1:
    Pop(&de_, memory);
    break;
  case 0xd2: Jump((*f_ & CARRY_FLAG) == 0, memory); break;
  case 0xd5:
    Push(de_, memory);
    break;
  case 0xd6:
    Sub8(a_, Read8(pc_, memory));
    pc_++;
    break;
  case 0xd8: if ((*f_ & CARRY_FLAG) > 0) Return(memory); break;
  case 0xd9:
    Return(memory);
    ime_ = true;  // Re-enable interrupts.
    break;
  case 0xde:
    {
      uint8_t value = Read8(pc_, memory);
      pc_++;
      SubCarry8(a_, value);
    }
    break;
  case 0xe0:
    {
      uint8_t addr = Read8(pc_, memory);
      pc_ += 1;
      Write8(0xff00 + addr, *a_, memory);
    }
    break;
  case 0xe1:
    Pop(&hl_, memory);
    break;
  case 0xe2:
    {
      Write8(0xff00 + (*c_), *a_, memory);
    }
    break;
  case 0xe5:
    Push(hl_, memory);
    break;
  case 0xe6:
    {
      uint8_t value = Read8(pc_, memory);
      pc_ += 1;
      And(value);
    }
    break;
  case 0xe9:
    pc_ = hl_;
    break;
  case 0xea:
    {
      uint16_t addr = Read16(pc_, memory);
      pc_ += 2;
      Write8(addr, *a_, memory);
    }
    break;
  case 0xee:
    Xor(Read8(pc_, memory));
    pc_++;
    break;
  case 0xef:
    Push(pc_, memory);
    pc_ = 0x28;
    break;
  case 0xf0:
    {
      uint8_t addr = Read8(pc_, memory);
      pc_ += 1;
      *a_ = Read8(0xff00 + addr, memory);
    }
    break;
  case 0xf1:
    Pop(&af_, memory);
    break;
  case 0xf3:
    ime_ = false;
    break;
  case 0xf5:
    Push(af_, memory);
    break;
  case 0xf6:
    Or(Read8(pc_, memory));
    pc_++;
    break;
  case 0xfa:
    {
      uint16_t addr = Read16(pc_, memory);
      pc_ += 2;
      *a_ = Read8(addr, memory);
    }
    break;
  case 0xfb:
    ime_ = true;
    break;
  case 0xfe:
    {
      uint8_t data = Read8(pc_, memory);
      pc_ += 1;

      // NOTE: this is a subtraction operation, but we throw away the result.
      *f_ = SUBTRACT_FLAG;  // Set subtraction flag.
      if (*a_ == data) *f_ |= ZERO_FLAG;
      if ((*a_ & 0xf) < (data & 0xf)) *f_ |= HALF_CARRY_FLAG;
      if (*a_ < data) *f_ |= CARRY_FLAG;
    }
    break;
  case 0xff:
    Push(pc_, memory);
    pc_ = 0x38;
    break;
  default:
    ERRORF("UNIMPLEMENTED (0x%02x): %s", code & 0xff, ops[code].debug.c_str());
    return false;
  }

  *cycle_count = ops[code].cycle_count;

  return true;
}

inline uint8_t CPU::ShiftLeft(uint8_t value) {
  // Shift bit 7 of *value into the carry flag position.
  *f_ = CARRY_FLAG & (value >> 3);
  uint8_t new_value = (value << 1);
  if (new_value == 0) *f_ |= ZERO_FLAG;
  return new_value;
}

inline void CPU::ShiftRight(uint8_t *value) {
  // Shift bit 0 of *value into the carry flag position.
  *f_ = CARRY_FLAG & (*value << 4);
  *value = (*value >> 1);
  if (*value == 0) *f_ |= ZERO_FLAG;
}

inline uint8_t CPU::Swap(uint8_t value) {
  uint8_t new_value = (value >> 4) | (value << 4);
  *f_ = (new_value == 0) ? ZERO_FLAG : 0;
  return new_value;
}

inline void CPU::TestBit(uint8_t value, unsigned int bit_index) {
  // Mask out everything but the target bit, shift it to the leftmost
  // position (zero flag), and invert that bit.
  uint8_t zero = ((value & (1 << bit_index)) << (7 - bit_index)) ^ 0x80;
  // Set the half-carry flag, reset the subtraction flag, and retain the old
  // carry flag value.
  *f_ = zero | 0x20 | (*f_ & CARRY_FLAG);
}

inline uint8_t CPU::SetBit(uint8_t value, unsigned int bit_index) {
  return value | (1 << bit_index);
}

inline uint8_t CPU::ResetBit(uint8_t value, unsigned int bit_index) {
  return value & (~(1 << bit_index));
}

bool CPU::RunPrefix(uint8_t code, Memory *memory) {
  switch (code) {
  case 0x00: *b_ = RotateLeft(*b_); break;
  case 0x01: *c_ = RotateLeft(*c_); break;
  case 0x02: *d_ = RotateLeft(*d_); break;
  case 0x03: *e_ = RotateLeft(*e_); break;
  case 0x04: *h_ = RotateLeft(*h_); break;
  case 0x05: *l_ = RotateLeft(*l_); break;
  case 0x06: Write8(hl_, RotateLeft(Read8(hl_, memory)), memory); break;
  case 0x07: *a_ = RotateLeft(*a_); break;

  case 0x08: *b_ = RotateRight(*b_); break;
  case 0x09: *c_ = RotateRight(*c_); break;
  case 0x0a: *d_ = RotateRight(*d_); break;
  case 0x0b: *e_ = RotateRight(*e_); break;
  case 0x0c: *h_ = RotateRight(*h_); break;
  case 0x0d: *l_ = RotateRight(*l_); break;
  case 0x0e: Write8(hl_, RotateRight(Read8(hl_, memory)), memory); break;
  case 0x0f: *a_ = RotateRight(*a_); break;

  case 0x10: *b_ = RotateLeftThroughCarry(*b_); break;
  case 0x11: *c_ = RotateLeftThroughCarry(*c_); break;
  case 0x12: *d_ = RotateLeftThroughCarry(*d_); break;
  case 0x13: *e_ = RotateLeftThroughCarry(*e_); break;
  case 0x14: *h_ = RotateLeftThroughCarry(*h_); break;
  case 0x15: *l_ = RotateLeftThroughCarry(*l_); break;
  case 0x16: Write8(hl_, RotateLeftThroughCarry(Read8(hl_, memory)), memory); break;
  case 0x17: *a_ = RotateLeftThroughCarry(*a_); break;

  case 0x18: *b_ = RotateRightThroughCarry(*b_); break;
  case 0x19: *c_ = RotateRightThroughCarry(*c_); break;
  case 0x1a: *d_ = RotateRightThroughCarry(*d_); break;
  case 0x1b: *e_ = RotateRightThroughCarry(*e_); break;
  case 0x1c: *h_ = RotateRightThroughCarry(*h_); break;
  case 0x1d: *l_ = RotateRightThroughCarry(*l_); break;
  case 0x1e: Write8(hl_, RotateRightThroughCarry(Read8(hl_, memory)), memory); break;
  case 0x1f: *a_ = RotateRightThroughCarry(*a_); break;

  case 0x20: *b_ = ShiftLeft(*b_); break;
  case 0x21: *c_ = ShiftLeft(*c_); break;
  case 0x22: *d_ = ShiftLeft(*d_); break;
  case 0x23: *e_ = ShiftLeft(*e_); break;
  case 0x24: *h_ = ShiftLeft(*h_); break;
  case 0x25: *l_ = ShiftLeft(*l_); break;
  case 0x26: Write8(hl_, ShiftLeft(Read8(hl_, memory)), memory); break;
  case 0x27: *a_ = ShiftLeft(*a_); break;

  case 0x30: *b_ = Swap(*b_); break;
  case 0x31: *c_ = Swap(*c_); break;
  case 0x32: *d_ = Swap(*d_); break;
  case 0x33: *e_ = Swap(*e_); break;
  case 0x34: *h_ = Swap(*h_); break;
  case 0x35: *l_ = Swap(*l_); break;
  case 0x36: Write8(hl_, Swap(Read8(hl_, memory)), memory); break;
  case 0x37: *a_ = Swap(*a_); break;

  case 0x3f: ShiftRight(a_); break;

  case 0x40: TestBit(*b_, 0); break;
  case 0x41: TestBit(*c_, 0); break;
  case 0x42: TestBit(*d_, 0); break;
  case 0x43: TestBit(*e_, 0); break;
  case 0x44: TestBit(*h_, 0); break;
  case 0x45: TestBit(*l_, 0); break;
  case 0x46: TestBit(Read8(hl_, memory), 0); break;
  case 0x47: TestBit(*a_, 0); break;

  case 0x48: TestBit(*b_, 1); break;
  case 0x49: TestBit(*c_, 1); break;
  case 0x4a: TestBit(*d_, 1); break;
  case 0x4b: TestBit(*e_, 1); break;
  case 0x4c: TestBit(*h_, 1); break;
  case 0x4d: TestBit(*l_, 1); break;
  case 0x4e: TestBit(Read8(hl_, memory), 1); break;
  case 0x4f: TestBit(*a_, 1); break;

  case 0x50: TestBit(*b_, 2); break;
  case 0x51: TestBit(*c_, 2); break;
  case 0x52: TestBit(*d_, 2); break;
  case 0x53: TestBit(*e_, 2); break;
  case 0x54: TestBit(*h_, 2); break;
  case 0x55: TestBit(*l_, 2); break;
  case 0x56: TestBit(Read8(hl_, memory), 2); break;
  case 0x57: TestBit(*a_, 2); break;

  case 0x58: TestBit(*b_, 3); break;
  case 0x59: TestBit(*c_, 3); break;
  case 0x5a: TestBit(*d_, 3); break;
  case 0x5b: TestBit(*e_, 3); break;
  case 0x5c: TestBit(*h_, 3); break;
  case 0x5d: TestBit(*l_, 3); break;
  case 0x5e: TestBit(Read8(hl_, memory), 3); break;
  case 0x5f: TestBit(*a_, 3); break;

  case 0x60: TestBit(*b_, 4); break;
  case 0x61: TestBit(*c_, 4); break;
  case 0x62: TestBit(*d_, 4); break;
  case 0x63: TestBit(*e_, 4); break;
  case 0x64: TestBit(*h_, 4); break;
  case 0x65: TestBit(*l_, 4); break;
  case 0x66: TestBit(Read8(hl_, memory), 4); break;
  case 0x67: TestBit(*a_, 4); break;

  case 0x68: TestBit(*b_, 5); break;
  case 0x69: TestBit(*c_, 5); break;
  case 0x6a: TestBit(*d_, 5); break;
  case 0x6b: TestBit(*e_, 5); break;
  case 0x6c: TestBit(*h_, 5); break;
  case 0x6d: TestBit(*l_, 5); break;
  case 0x6e: TestBit(Read8(hl_, memory), 5); break;
  case 0x6f: TestBit(*a_, 5); break;

  case 0x70: TestBit(*b_, 6); break;
  case 0x71: TestBit(*c_, 6); break;
  case 0x72: TestBit(*d_, 6); break;
  case 0x73: TestBit(*e_, 6); break;
  case 0x74: TestBit(*h_, 6); break;
  case 0x75: TestBit(*l_, 6); break;
  case 0x76: TestBit(Read8(hl_, memory), 6); break;
  case 0x77: TestBit(*a_, 6); break;

  case 0x78: TestBit(*b_, 7); break;
  case 0x79: TestBit(*c_, 7); break;
  case 0x7a: TestBit(*d_, 7); break;
  case 0x7b: TestBit(*e_, 7); break;
  case 0x7c: TestBit(*h_, 7); break;
  case 0x7d: TestBit(*l_, 7); break;
  case 0x7e: TestBit(Read8(hl_, memory), 7); break;
  case 0x7f: TestBit(*a_, 7); break;

  case 0x80: *b_ = ResetBit(*b_, 0); break;
  case 0x81: *c_ = ResetBit(*c_, 0); break;
  case 0x82: *d_ = ResetBit(*d_, 0); break;
  case 0x83: *e_ = ResetBit(*e_, 0); break;
  case 0x84: *h_ = ResetBit(*h_, 0); break;
  case 0x85: *l_ = ResetBit(*l_, 0); break;
  case 0x86: Write8(hl_, ResetBit(Read8(hl_, memory), 0), memory); break;
  case 0x87: *a_ = ResetBit(*a_, 0); break;

  case 0x88: *b_ = ResetBit(*b_, 1); break;
  case 0x89: *c_ = ResetBit(*c_, 1); break;
  case 0x8a: *d_ = ResetBit(*d_, 1); break;
  case 0x8b: *e_ = ResetBit(*e_, 1); break;
  case 0x8c: *h_ = ResetBit(*h_, 1); break;
  case 0x8d: *l_ = ResetBit(*l_, 1); break;
  case 0x8e: Write8(hl_, ResetBit(Read8(hl_, memory), 1), memory); break;
  case 0x8f: *a_ = ResetBit(*a_, 1); break;

  case 0x90: *b_ = ResetBit(*b_, 2); break;
  case 0x91: *c_ = ResetBit(*c_, 2); break;
  case 0x92: *d_ = ResetBit(*d_, 2); break;
  case 0x93: *e_ = ResetBit(*e_, 2); break;
  case 0x94: *h_ = ResetBit(*h_, 2); break;
  case 0x95: *l_ = ResetBit(*l_, 2); break;
  case 0x96: Write8(hl_, ResetBit(Read8(hl_, memory), 2), memory); break;
  case 0x97: *a_ = ResetBit(*a_, 2); break;

  case 0x98: *b_ = ResetBit(*b_, 3); break;
  case 0x99: *c_ = ResetBit(*c_, 3); break;
  case 0x9a: *d_ = ResetBit(*d_, 3); break;
  case 0x9b: *e_ = ResetBit(*e_, 3); break;
  case 0x9c: *h_ = ResetBit(*h_, 3); break;
  case 0x9d: *l_ = ResetBit(*l_, 3); break;
  case 0x9e: Write8(hl_, ResetBit(Read8(hl_, memory), 3), memory); break;
  case 0x9f: *a_ = ResetBit(*a_, 3); break;

  case 0xa0: *b_ = ResetBit(*b_, 4); break;
  case 0xa1: *c_ = ResetBit(*c_, 4); break;
  case 0xa2: *d_ = ResetBit(*d_, 4); break;
  case 0xa3: *e_ = ResetBit(*e_, 4); break;
  case 0xa4: *h_ = ResetBit(*h_, 4); break;
  case 0xa5: *l_ = ResetBit(*l_, 4); break;
  case 0xa6: Write8(hl_, ResetBit(Read8(hl_, memory), 4), memory); break;
  case 0xa7: *a_ = ResetBit(*a_, 4); break;

  case 0xa8: *b_ = ResetBit(*b_, 5); break;
  case 0xa9: *c_ = ResetBit(*c_, 5); break;
  case 0xaa: *d_ = ResetBit(*d_, 5); break;
  case 0xab: *e_ = ResetBit(*e_, 5); break;
  case 0xac: *h_ = ResetBit(*h_, 5); break;
  case 0xad: *l_ = ResetBit(*l_, 5); break;
  case 0xae: Write8(hl_, ResetBit(Read8(hl_, memory), 5), memory); break;
  case 0xaf: *a_ = ResetBit(*a_, 5); break;

  case 0xb0: *b_ = ResetBit(*b_, 6); break;
  case 0xb1: *c_ = ResetBit(*c_, 6); break;
  case 0xb2: *d_ = ResetBit(*d_, 6); break;
  case 0xb3: *e_ = ResetBit(*e_, 6); break;
  case 0xb4: *h_ = ResetBit(*h_, 6); break;
  case 0xb5: *l_ = ResetBit(*l_, 6); break;
  case 0xb6: Write8(hl_, ResetBit(Read8(hl_, memory), 6), memory); break;
  case 0xb7: *a_ = ResetBit(*a_, 6); break;

  case 0xb8: *b_ = ResetBit(*b_, 7); break;
  case 0xb9: *c_ = ResetBit(*c_, 7); break;
  case 0xba: *d_ = ResetBit(*d_, 7); break;
  case 0xbb: *e_ = ResetBit(*e_, 7); break;
  case 0xbc: *h_ = ResetBit(*h_, 7); break;
  case 0xbd: *l_ = ResetBit(*l_, 7); break;
  case 0xbe: Write8(hl_, ResetBit(Read8(hl_, memory), 7), memory); break;
  case 0xbf: *a_ = ResetBit(*a_, 7); break;

  case 0xc0: *b_ = SetBit(*b_, 0); break;
  case 0xc1: *c_ = SetBit(*c_, 0); break;
  case 0xc2: *d_ = SetBit(*d_, 0); break;
  case 0xc3: *e_ = SetBit(*e_, 0); break;
  case 0xc4: *h_ = SetBit(*h_, 0); break;
  case 0xc5: *l_ = SetBit(*l_, 0); break;
  case 0xc6: Write8(hl_, SetBit(Read8(hl_, memory), 0), memory); break;
  case 0xc7: *a_ = SetBit(*a_, 0); break;

  case 0xc8: *b_ = SetBit(*b_, 1); break;
  case 0xc9: *c_ = SetBit(*c_, 1); break;
  case 0xca: *d_ = SetBit(*d_, 1); break;
  case 0xcb: *e_ = SetBit(*e_, 1); break;
  case 0xcc: *h_ = SetBit(*h_, 1); break;
  case 0xcd: *l_ = SetBit(*l_, 1); break;
  case 0xce: Write8(hl_, SetBit(Read8(hl_, memory), 1), memory); break;
  case 0xcf: *a_ = SetBit(*a_, 1); break;

  case 0xd0: *b_ = SetBit(*b_, 2); break;
  case 0xd1: *c_ = SetBit(*c_, 2); break;
  case 0xd2: *d_ = SetBit(*d_, 2); break;
  case 0xd3: *e_ = SetBit(*e_, 2); break;
  case 0xd4: *h_ = SetBit(*h_, 2); break;
  case 0xd5: *l_ = SetBit(*l_, 2); break;
  case 0xd6: Write8(hl_, SetBit(Read8(hl_, memory), 2), memory); break;
  case 0xd7: *a_ = SetBit(*a_, 2); break;

  case 0xd8: *b_ = SetBit(*b_, 3); break;
  case 0xd9: *c_ = SetBit(*c_, 3); break;
  case 0xda: *d_ = SetBit(*d_, 3); break;
  case 0xdb: *e_ = SetBit(*e_, 3); break;
  case 0xdc: *h_ = SetBit(*h_, 3); break;
  case 0xdd: *l_ = SetBit(*l_, 3); break;
  case 0xde: Write8(hl_, SetBit(Read8(hl_, memory), 3), memory); break;
  case 0xdf: *a_ = SetBit(*a_, 3); break;

  case 0xe0: *b_ = SetBit(*b_, 4); break;
  case 0xe1: *c_ = SetBit(*c_, 4); break;
  case 0xe2: *d_ = SetBit(*d_, 4); break;
  case 0xe3: *e_ = SetBit(*e_, 4); break;
  case 0xe4: *h_ = SetBit(*h_, 4); break;
  case 0xe5: *l_ = SetBit(*l_, 4); break;
  case 0xe6: Write8(hl_, SetBit(Read8(hl_, memory), 4), memory); break;
  case 0xe7: *a_ = SetBit(*a_, 4); break;

  case 0xe8: *b_ = SetBit(*b_, 5); break;
  case 0xe9: *c_ = SetBit(*c_, 5); break;
  case 0xea: *d_ = SetBit(*d_, 5); break;
  case 0xeb: *e_ = SetBit(*e_, 5); break;
  case 0xec: *h_ = SetBit(*h_, 5); break;
  case 0xed: *l_ = SetBit(*l_, 5); break;
  case 0xee: Write8(hl_, SetBit(Read8(hl_, memory), 5), memory); break;
  case 0xef: *a_ = SetBit(*a_, 5); break;

  case 0xf0: *b_ = SetBit(*b_, 6); break;
  case 0xf1: *c_ = SetBit(*c_, 6); break;
  case 0xf2: *d_ = SetBit(*d_, 6); break;
  case 0xf3: *e_ = SetBit(*e_, 6); break;
  case 0xf4: *h_ = SetBit(*h_, 6); break;
  case 0xf5: *l_ = SetBit(*l_, 6); break;
  case 0xf6: Write8(hl_, SetBit(Read8(hl_, memory), 6), memory); break;
  case 0xf7: *a_ = SetBit(*a_, 6); break;

  case 0xf8: *b_ = SetBit(*b_, 7); break;
  case 0xf9: *c_ = SetBit(*c_, 7); break;
  case 0xfa: *d_ = SetBit(*d_, 7); break;
  case 0xfb: *e_ = SetBit(*e_, 7); break;
  case 0xfc: *h_ = SetBit(*h_, 7); break;
  case 0xfd: *l_ = SetBit(*l_, 7); break;
  case 0xfe: Write8(hl_, SetBit(Read8(hl_, memory), 7), memory); break;
  case 0xff: *a_ = SetBit(*a_, 7); break;

  default:
    ERRORF("UNIMPLEMENTED CB PREFIX (0x%02x): %s", code,
        cb_ops[code].debug.c_str());
    return false;
  }
  return true;
}

}  // namespace dgb
