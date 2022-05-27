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

  return 0;
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

  return 0;
}
