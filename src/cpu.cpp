#include "cpu.h"

#include <cstdint>
#include <functional>
#include <iostream>

#include "logging.h"
#include "memory.h"

namespace dgb {

// Loads 8 bits of data.
inline uint8_t CPU::LoadData8(uint8_t *dest, Memory *memory) {
  uint8_t data = memory->Read8(pc_);
  pc_ += 1;
  *dest = data;
  return data;
}

// Loads 8 bits of data to a memory address.
inline uint8_t CPU::LoadData8ToMem(uint16_t dest_addr, Memory *memory) {
  uint8_t data = memory->Read8(pc_);
  pc_ += 1;
  memory->Write8(dest_addr, data);
  return data;
}

// Loads 16 bits of data.
inline uint16_t CPU::LoadData16(uint16_t *dest, Memory *memory) {
  uint16_t data = memory->Read16(pc_);
  pc_ += 2;
  *dest = data;
  return data;
}

// Loads 8 bits from memory.
inline void CPU::LoadMem8(uint8_t *dest, uint16_t addr, Memory *memory) {
  uint8_t data = memory->Read8(addr);
  *dest = data;
}

// Copies 8 bits from one register to another.
inline void CPU::LoadReg8(uint8_t *dest, uint8_t value) {
  *dest = value;
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
  memory->Write16(sp_ - 2, value);
  sp_ -= 2;
}

// Pops a 16-bit value off the stack and into the given register.
inline void CPU::Pop(uint16_t *dest, Memory *memory) {
  *dest = memory->Read16(sp_);
  sp_ += 2;
}

inline void CPU::Jump(bool do_jump, Memory *memory) {
  uint16_t addr = memory->Read16(pc_);
  pc_ += 2;
  if (do_jump) {
    pc_ = addr;
  }
}

inline uint8_t CPU::JumpRelative(bool do_jump, Memory *memory) {
  // Use a signed byte here so we get subtraction too.
  int8_t jump = static_cast<int8_t>(memory->Read8(pc_));
  pc_ += 1;
  if (do_jump) {
    pc_ += jump;
  }
  return jump;
}

inline void CPU::Return(bool do_return, Memory *memory) {
  uint16_t addr = memory->Read16(sp_);
  sp_ += 2;
  if (do_return) {
    pc_ = addr;
  }
}

// Does a bitwise left rotation, using the carry bit as a 9th bit in the
// operation.
// Does NOT advance the program counter.
inline void CPU::RotateLeftThroughCarry(uint8_t *value) {
  uint8_t left_bit = *value & 0x80;
  // Shift left, filling rightmost bit with carry flag's value.
  *value = (*value << 1) | ((*f_ & CARRY_FLAG) >> 4);
  // Set the carry flag to the old value's 8th bit, and set the zero flag if
  // the result was zero.
  uint8_t zero_bit = (*value == 0) ? ZERO_FLAG : 0;
  *f_ = (left_bit >> 3) | zero_bit;
}

bool CPU::ProcessInterrupts(Memory *memory) {
  // TODO: move these registers into the CPU class
  uint8_t interrupt_enable = memory->Read8(0xFF0F);
  uint8_t interrupt_request = memory->Read8(0xFFFF);
  uint8_t interrupts = interrupt_enable & interrupt_request;
  for (int i = 0; i < 5; ++i) {
    if ((interrupts & (1 << i)) > 0) {
      // Transfer execution to the requested interrupt handler.
      Push(pc_, memory);
      pc_ = kInterruptHandlers[i];
      ime_ = false;
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

  if (pc_ == breakpoint_) {
    debug_ = true;
  }

  // Consume the opcode and advance the program counter.
  uint8_t code = memory->Read8(pc_);
  if (!memory->bootstrap_is_mapped() || debug_) {
    PrintRegisters();

    Op op = ops[code];
    if (op.length == 3) {
      DEBUGF("[0x%04x]: 0x%02x 0x%02x 0x%02x %s",
          pc_ & 0xffff, code & 0xff,
          memory->Read8(pc_ + 1) & 0xff,
          memory->Read8(pc_ + 2) & 0xff,
          op.debug.c_str());
    } else if (op.length == 2) {
      DEBUGF("[0x%04x]: 0x%02x 0x%02x     %s",
          pc_ & 0xffff, code & 0xff,
          memory->Read8(pc_ + 1) & 0xff,
          op.debug.c_str());
    } else {
      DEBUGF("[0x%04x]: 0x%02x          %s",
          pc_ & 0xffff, code & 0xff, op.debug.c_str());
    }
  }

  if (debug_) {
    // TODO: formalize this as a debugger
    std::string line;
    while (true) {
      getline(std::cin, line);
      if (line.length() == 0) {
        break;
      }

      if (std::string("print").compare(line.substr(0, 5)) == 0) {
        int addr = stoi(line.substr(6), 0, 16);
        if (addr > 0xfffe) {
          WARNINGF("Cannot read memory at address 0x%x", addr);
        } else {
          uint16_t val = memory->Read16(addr);
          printf("[0x%04x]: 0x%02x 0x%02x\n",
              addr & 0xffff, val & 0xff, val >> 8);
        }
      } else if (std::string("break").compare(line.substr(0, 5)) == 0) {
        breakpoint_ = stoi(line.substr(6), 0, 16);
        debug_ = false;
        break;
      } else if (std::string("quit").compare(line.substr(0, 4)) == 0) {
        exit(0);
      }
    }
  }
  pc_++;


  switch (code) {
  case 0x00:
    break;
  case 0x01:
    LoadData16(&bc_, memory);
    break;
  case 0x04:
    Inc8(b_);
    break;
  case 0x05:
    Dec8(b_);
    break;
  case 0x06:
    LoadData8(b_, memory);
    break;
  case 0x0b:
    Dec16(&bc_);
    break;
  case 0x0c:
    Inc8(c_);
    break;
  case 0x0d:
    Dec8(c_);
    break;
  case 0x0e:
    LoadData8(c_, memory);
    break;
  case 0x11:
    LoadData16(&de_, memory);
    break;
  case 0x12:
    LoadData8ToMem(de_, memory);
    break;
  case 0x13:
    Inc16(&de_);
    break;
  case 0x15:
    Dec8(d_);
    break;
  case 0x16:
    LoadData8(d_, memory);
    break;
  case 0x17:
    RotateLeftThroughCarry(a_);
    break;
  case 0x18:
    JumpRelative(true, memory);
    break;
  case 0x19:
    Add16(&hl_, de_);
    break;
  case 0x1a:
    LoadMem8(a_, de_, memory);
    break;
  case 0x1c:
    Inc8(e_);
    break;
  case 0x1d:
    Dec8(e_);
    break;
  case 0x1e:
    LoadData8(e_, memory);
    break;
  case 0x20:
    JumpRelative(((*f_ & ZERO_FLAG) == 0), memory);
    break;
  case 0x21:
    LoadData16(&hl_, memory);
    break;
  case 0x22:
    memory->Write8(hl_, *a_);
    hl_ += 1;
    break;
  case 0x23:
    Inc16(&hl_);
    break;
  case 0x24:
    Inc8(h_);
    break;
  case 0x28:
    JumpRelative(((*f_ & ZERO_FLAG) != 0), memory);
    break;
  case 0x2a:
    *a_ = memory->Read8(hl_);
    hl_ += 1;
    break;
  case 0x2e:
    LoadData8(l_, memory);
    break;
  case 0x2f:
    *a_ = ~(*a_);
    *f_ = SUBTRACT_FLAG | HALF_CARRY_FLAG;
    break;
  case 0x31:
    LoadData16(&sp_, memory);
    break;
  case 0x32:
    memory->Write8(hl_, *a_);
    hl_ -= 1;
    break;
  case 0x36:
    LoadData8ToMem(hl_, memory);
    break;
  case 0x3d:
    Dec8(a_);
    break;
  case 0x3e:
    LoadData8(a_, memory);
    break;
  case 0x47:
    LoadReg8(b_, *a_);
    break;
  case 0x4f:
    LoadReg8(c_, *a_);
    break;
  case 0x56:
    LoadMem8(d_, hl_, memory);
    break;
  case 0x57:
    LoadReg8(d_, *a_);
    break;
  case 0x5e:
    LoadMem8(e_, hl_, memory);
    break;
  case 0x5f:
    LoadReg8(e_, *a_);
    break;
  case 0x67:
    LoadReg8(h_, *a_);
    break;
  case 0x77:
    memory->Write8(hl_, *a_);
    break;
  case 0x78:
    LoadReg8(a_, *b_);
    break;
  case 0x79:
    LoadReg8(a_, *c_);
    break;
  case 0x7a:
    LoadReg8(a_, *d_);
    break;
  case 0x7b:
    LoadReg8(a_, *e_);
    break;
  case 0x7c:
    LoadReg8(a_, *h_);
    break;
  case 0x7d:
    LoadReg8(a_, *l_);
    break;
  case 0x7e:
    LoadMem8(a_, hl_, memory);
    break;
  case 0x86:
    {
      uint8_t data = memory->Read8(hl_);
      Add8(a_, data);
    }
    break;
  case 0x87:
    Add8(a_, *a_);
    break;
  case 0x90:
    Sub8(a_, *b_);
    break;
  case 0xa1:
    And(*c_);
    break;
  case 0xa7:
    And(*a_);
    break;
  case 0xa9:
    Xor(*c_);
    break;
  case 0xaf:
    Xor(*a_);
    break;
  case 0xb0:
    Or(*b_);
    break;
  case 0xb1:
    Or(*c_);
    break;
  case 0xbe:
    {
      uint8_t data = memory->Read8(hl_);

      // NOTE: this is a subtraction operation, but we throw away the result.
      *f_ = SUBTRACT_FLAG;  // Set subtraction flag.
      if (*a_ == data) *f_ |= ZERO_FLAG;
      if ((*a_ & 0xff) < (data & 0xff)) *f_ |= HALF_CARRY_FLAG;
      if (*a_ < data) *f_ |= CARRY_FLAG;
    }
    break;
  case 0xc0:
    Return((*f_ & ZERO_FLAG) == 0, memory);
    break;
  case 0xc1:
    Pop(&bc_, memory);
    break;
  case 0xc3:
    {
      uint16_t addr = memory->Read16(pc_);
      pc_ = addr;
    }
    break;
  case 0xc5:
    Push(bc_, memory);
    break;
  case 0xc8:
    Return((*f_ & ZERO_FLAG) > 0, memory);
    break;
  case 0xc9:
    Return(true, memory);
    break;
  case 0xca:
    Jump((*f_ & ZERO_FLAG) > 0, memory);
    break;
  case 0xcb:
    {
      // Consume the code and advance the program counter.
      uint8_t code = memory->Read8(pc_);
      pc_++;
      return RunPrefix(code, memory);
    }
    break;  // Unreachable.
  case 0xcd:
    {
      uint16_t addr = memory->Read16(pc_);
      pc_ += 2;
      memory->Write16(sp_ - 2, pc_);
      sp_ -= 2;
      pc_ = addr;
    }
    break;
  case 0xd1:
    Pop(&de_, memory);
    break;
  case 0xd5:
    Push(de_, memory);
    break;
  case 0xe0:
    {
      uint8_t addr = memory->Read8(pc_);
      pc_ += 1;
      memory->Write8(0xff00 + addr, *a_);
    }
    break;
  case 0xe1:
    Pop(&hl_, memory);
    break;
  case 0xe2:
    {
      memory->Write8(0xff00 + (*c_), *a_);
    }
    break;
  case 0xe5:
    Push(hl_, memory);
    break;
  case 0xe6:
    {
      uint8_t value = memory->Read8(pc_);
      pc_ += 1;
      And(value);
    }
    break;
  case 0xe9:
    pc_ = hl_;
    break;
  case 0xea:
    {
      uint16_t addr = memory->Read16(pc_);
      pc_ += 2;
      memory->Write8(addr, *a_);
    }
    break;
  case 0xef:
    Push(pc_, memory);
    pc_ = 0x28;
    break;
  case 0xf0:
    {
      uint8_t addr = memory->Read8(pc_);
      pc_ += 1;
      *a_ = memory->Read8(0xff00 + addr);
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
  case 0xfa:
    {
      uint16_t addr = memory->Read16(pc_);
      pc_ += 2;
      LoadMem8(a_, addr, memory);
    }
    break;
  case 0xfb:
    ime_ = true;
    break;
  case 0xfe:
    {
      uint8_t data = memory->Read8(pc_);
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

inline void CPU::Swap(uint8_t *dest, uint8_t value) {
  *dest = (value >> 4) | (value << 4);
  *f_ = (*dest == 0) ? ZERO_FLAG : 0;
}

inline void CPU::TestBit(uint8_t value, unsigned int bit_index) {
  // Mask out everything but the target bit, shift it to the leftmost
  // position (zero flag), and invert that bit.
  uint8_t zero = ((value & (1 << bit_index)) << (7 - bit_index)) ^ 0x80;
  // Set the half-carry flag, reset the subtraction flag, and retain the old
  // carry flag value.
  *f_ = zero | 0x20 | (*f_ & CARRY_FLAG);
}

inline void CPU::SetBit(uint8_t *dest, unsigned int bit_index) {
  *dest |= (1 << bit_index);
}

inline void CPU::ResetBit(uint8_t *dest, unsigned int bit_index) {
  *dest &= (~(1 << bit_index));
}

bool CPU::RunPrefix(uint8_t code, Memory *memory) {
  switch (code) {
  case 0x11:
    RotateLeftThroughCarry(c_);
    break;
  case 0x37:
    Swap(a_, *a_);
    break;
  case 0x7c:
    TestBit(*h_, 7);
    break;
  case 0x87:
    ResetBit(a_, 0);
    break;
  default:
    ERRORF("UNIMPLEMENTED CB PREFIX (0x%02x): %s", code,
        cb_ops[code].debug.c_str());
    return false;
  }
  return true;
}

}  // namespace dgb
