#include "common/test/test.h"

#include "cpu.h"

#include <memory>

#include "clock.h"
#include "interrupts.h"
#include "memory.h"

using dgb::Clock;
using dgb::TestCPU;
using dgb::CPU;
using dgb::Interrupts;
using dgb::MockMemoryBus;

TEST(CPU_OP_0x05) {
  std::shared_ptr<Clock> clock = std::make_shared<Clock>(false);  // no throttling b/c it doesn't matter in a test
  std::shared_ptr<Interrupts> interrupts = std::make_shared<Interrupts>();
  TestCPU cpu(clock, interrupts);
  MockMemoryBus memory_bus;

  cpu.set_sp(0x1234);
  ASSERT_EQ(cpu.get_sp(), 0x1234);

  int cycle_count = 0;

  memory_bus.ExpectRead(0x0, 0x08);  // opcode
  memory_bus.ExpectRead(0x1, 0xcd);  // address low bits
  memory_bus.ExpectRead(0x2, 0xab);  // address high bits
  memory_bus.ExpectWrite(0xabcd, 0x34);  // write low byte
  memory_bus.ExpectWrite(0xabce, 0x12);  // write high byte
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1234);  // sp register stays the same
  // TODO: assert registers are untouched
  ASSERT_EQ(cycle_count, 20);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  return 0;  // Necessary for tests to pass
}

// LD (a16),SP
TEST(CPU_OP_0x08) {
  std::shared_ptr<Clock> clock = std::make_shared<Clock>(false);  // no throttling b/c it doesn't matter in a test
  std::shared_ptr<Interrupts> interrupts = std::make_shared<Interrupts>();
  TestCPU cpu(clock, interrupts);
  MockMemoryBus memory_bus;

  cpu.set_b(0x3);
  ASSERT_EQ(cpu.get_b(), 0x3);

  int cycle_count = 0;

  memory_bus.ExpectRead(0x0, 0x05);  // opcode
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_b(), 0x2);
  ASSERT_EQ(cpu.get_f(), CPU::SUBTRACT_FLAG);
  ASSERT_EQ(cycle_count, 4);

  memory_bus.ExpectRead(0x1, 0x05);  // opcode
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_b(), 0x1);
  ASSERT_EQ(cpu.get_f(), CPU::SUBTRACT_FLAG);
  ASSERT_EQ(cycle_count, 4);

  memory_bus.ExpectRead(0x2, 0x05);  // opcode
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_b(), 0x0);
  ASSERT_EQ(cpu.get_f(), CPU::ZERO_FLAG | CPU::SUBTRACT_FLAG);
  ASSERT_EQ(cycle_count, 4);

  memory_bus.ExpectRead(0x3, 0x05);  // opcode
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_b(), 0xff);
  ASSERT_EQ(cpu.get_f(), CPU::HALF_CARRY_FLAG | CPU::SUBTRACT_FLAG);
  ASSERT_EQ(cycle_count, 4);

  cpu.set_b(0x10);
  memory_bus.ExpectRead(0x4, 0x05);  // opcode
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_b(), 0x0f);
  ASSERT_EQ(cpu.get_f(), CPU::HALF_CARRY_FLAG | CPU::SUBTRACT_FLAG);
  ASSERT_EQ(cycle_count, 4);

  return 0;  // Necessary for tests to pass
}

TEST(CPU_OP_0xce) {
  std::shared_ptr<Clock> clock = std::make_shared<Clock>(false);  // no throttling b/c it doesn't matter in a test
  std::shared_ptr<Interrupts> interrupts = std::make_shared<Interrupts>();
  TestCPU cpu(clock, interrupts);
  MockMemoryBus memory_bus;

  memory_bus.ExpectRead(0x0, 0xce);  // opcode
  memory_bus.ExpectRead(0x1, 0x07);  // data

  int cycle_count = 0;
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));

  ASSERT_EQ(cpu.get_a(), 0x7);
  // TODO: check other registers

  // Load register A into memory so we can expect a write
  memory_bus.ExpectRead(0x2, 0xea);  // opcode: Load (a16) <- A
  memory_bus.ExpectRead(0x3, 0x01);  // address pt 1
  memory_bus.ExpectRead(0x4, 0x02);  // address pt 2
  memory_bus.ExpectWrite(0x0201, 0x07);  // expect value in register A to be written
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));

  return 0;  // Necessary for tests to pass
}

// ADD SP,r8
TEST(CPU_OP_0xe8) {
  std::shared_ptr<Clock> clock = std::make_shared<Clock>(false);  // no throttling b/c it doesn't matter in a test
  std::shared_ptr<Interrupts> interrupts = std::make_shared<Interrupts>();
  TestCPU cpu(clock, interrupts);
  MockMemoryBus memory_bus;

  cpu.set_sp(0x1234);
  ASSERT_EQ(cpu.get_sp(), 0x1234);

  int cycle_count = 0;
  int pc = 0;
  const uint8_t op = 0xe8;
  const uint8_t want_cycles = 16;

  ///// Add
  memory_bus.ExpectRead(pc++, op);  // opcode
  memory_bus.ExpectRead(pc++, 0x12);  // data
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1246);
  ASSERT_EQ(cpu.get_f(), 0);
  ASSERT_EQ(cycle_count, want_cycles);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  ///// Subtract
  cpu.set_sp(0x1234);
  memory_bus.ExpectRead(pc++, op);  // opcode
  memory_bus.ExpectRead(pc++, 0xee);  // data (-0x12)
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1222);
  ASSERT_EQ(cpu.get_f(), 0);
  ASSERT_EQ(cycle_count, want_cycles);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  ///// Add with carry and half carry
  cpu.set_sp(0x12fe);
  memory_bus.ExpectRead(pc++, op);  // opcode
  memory_bus.ExpectRead(pc++, 0x03);  // data
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1301);
  ASSERT_EQ(cpu.get_f(), CPU::CARRY_FLAG | CPU::HALF_CARRY_FLAG);
  ASSERT_EQ(cycle_count, want_cycles);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  ///// Subtract with carry and half carry
  cpu.set_sp(0x1201);
  memory_bus.ExpectRead(pc++, op);  // opcode
  memory_bus.ExpectRead(pc++, 0xfd);  // data
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x11fe);  // hl register modified
  ASSERT_EQ(cpu.get_f(), CPU::CARRY_FLAG | CPU::HALF_CARRY_FLAG);
  ASSERT_EQ(cycle_count, want_cycles);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  return 0;  // Necessary for tests to pass
}

// LD HL,SP+r8
TEST(CPU_OP_0xf8) {
  std::shared_ptr<Clock> clock = std::make_shared<Clock>(false);  // no throttling b/c it doesn't matter in a test
  std::shared_ptr<Interrupts> interrupts = std::make_shared<Interrupts>();
  TestCPU cpu(clock, interrupts);
  MockMemoryBus memory_bus;

  cpu.set_hl(0x0000);
  cpu.set_sp(0x1234);
  ASSERT_EQ(cpu.get_hl(), 0x0000);
  ASSERT_EQ(cpu.get_sp(), 0x1234);

  int cycle_count = 0;
  int pc = 0;

  ///// Add
  memory_bus.ExpectRead(pc++, 0xf8);  // opcode
  memory_bus.ExpectRead(pc++, 0x12);  // data
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1234);  // sp register stays the same
  ASSERT_EQ(cpu.get_hl(), 0x1246);  // hl register modified
  ASSERT_EQ(cpu.get_f(), 0);
  ASSERT_EQ(cycle_count, 12);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  ///// Subtract
  cpu.set_hl(0x0000);
  cpu.set_sp(0x1234);
  memory_bus.ExpectRead(pc++, 0xf8);  // opcode
  memory_bus.ExpectRead(pc++, 0xee);  // data (-0x12)
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1234);  // sp register stays the same
  ASSERT_EQ(cpu.get_hl(), 0x1222);  // hl register modified
  ASSERT_EQ(cpu.get_f(), 0);
  ASSERT_EQ(cycle_count, 12);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  ///// Add with carry and half carry
  cpu.set_hl(0x0000);
  cpu.set_sp(0x12fe);
  memory_bus.ExpectRead(pc++, 0xf8);  // opcode
  memory_bus.ExpectRead(pc++, 0x03);  // data
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x12fe);  // sp register stays the same
  ASSERT_EQ(cpu.get_hl(), 0x1301);  // hl register modified
  ASSERT_EQ(cpu.get_f(), CPU::CARRY_FLAG | CPU::HALF_CARRY_FLAG);
  ASSERT_EQ(cycle_count, 12);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  ///// Subtract with carry and half carry
  cpu.set_hl(0x0000);
  cpu.set_sp(0x1201);
  memory_bus.ExpectRead(pc++, 0xf8);  // opcode
  memory_bus.ExpectRead(pc++, 0xfd);  // data
  ASSERT_TRUE(cpu.RunOp(&memory_bus, &cycle_count));
  ASSERT_EQ(cpu.get_sp(), 0x1201);  // sp register stays the same
  ASSERT_EQ(cpu.get_hl(), 0x11fe);  // hl register modified
  ASSERT_EQ(cpu.get_f(), CPU::CARRY_FLAG | CPU::HALF_CARRY_FLAG);
  ASSERT_EQ(cycle_count, 12);
  // Assert that all expected reads/writes have been run
  ASSERT_EQ(memory_bus.RemainingExpectations(), 0);

  return 0;  // Necessary for tests to pass
}
