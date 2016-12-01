#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <SDL2/SDL.h>

#include "cartridge.h"
#include "clock.h"
#include "cpu.h"
#include "display.h"
#include "fixes.h"
#include "flags.h"
#include "logging.h"
#include "memory.h"


using dgb::Cartridge;
using dgb::Clock;
using dgb::CPU;
using dgb::Display;
using dgb::Input;
using dgb::Interrupts;
using dgb::Memory;

struct {
  std::string filename;
} args;

void PrintHelpAndExit(char *arg0) {
  printf("Usage: %s <filename>\n", arg0);
  exit(0);
}

Flag<std::string> FLAG_bootloader("bootloader", "");
Flag<std::string> FLAG_breakpoint("breakpoint", "");

void ProcessArgs(int *argc, char **argv[]) {
  ParseFlags(argc, argv);
  if (*argc < 2) {
    PrintHelpAndExit((*argv)[0]);
  }
  args.filename = std::string((*argv)[1]);
}

int main(int argc, char *argv[]) {
  ProcessArgs(&argc, &argv);

  // TODO: don't require a bootloader file
  if ((*FLAG_bootloader).empty()) {
    FATALF("Missing bootloader file.");
  }

  std::shared_ptr<Clock> clock(new Clock());
  std::shared_ptr<Interrupts> interrupts(new Interrupts());
  std::shared_ptr<Display> display(new Display(512, 512, clock, interrupts));
  std::shared_ptr<Cartridge> cartridge(new Cartridge(args.filename));
  std::shared_ptr<Input> input(new Input());
  Memory memory(*FLAG_bootloader, cartridge, display, input);
  CPU cpu(clock, interrupts);

  if (cartridge->Title().compare("TETRIS") == 0)
    dgb::FIX_tetris = true;

  cpu.set_pc(0x100);
  memory.set_bootstrap_is_mapped(false);

  if (!(*FLAG_breakpoint).empty()) {
    // NOTE: the breakpoint is interpreted as a hex string
    cpu.set_breakpoint(std::stoi(*FLAG_breakpoint, 0, 16));
  }

  cartridge->PrintCartridgeDebug();

  cpu.StartLoop(&memory);
  display->Loop();
  cpu.Kill();
  cpu.Wait();

	return 0;
}
