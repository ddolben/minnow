#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <SDL2/SDL.h>

#include "cartridge.h"
#include "clock.h"
#include "common/flags.h"
#include "cpu.h"
#include "display.h"
#include "event_dispatch.h"
#include "fixes.h"
#include "common/logging.h"
#include "memory.h"
#include "sound_controller.h"
#include "window.h"
#include "window_controller.h"


using dgb::Cartridge;
using dgb::Clock;
using dgb::CPU;
using dgb::Display;
using dgb::Event;
using dgb::EventDispatch;
using dgb::Input;
using dgb::Interrupts;
using dgb::Memory;
using dgb::SoundController;
using dgb::Timers;
using dgb::WindowController;

const char *kSaveExtension = ".sav";

struct {
  std::string filename;
} args;

void PrintHelpAndExit(char *arg0) {
  printf("Usage: %s <filename>\n", arg0);
  exit(0);
}

Flag<std::string> FLAG_bootloader("bootloader", "");
Flag<std::string> FLAG_breakpoint("breakpoint", "");
Flag<std::string> FLAG_breakpoint_opcode("breakpoint_opcode", "");
// Print the display framerate to the console.
Flag<bool> FLAG_print_fps("print_fps", false);
// Show debug windows, such as the tileset and the background map.
Flag<bool> FLAG_debug_windows("debug_windows", false);
// Throttle the CPU to run at native speed. Set to false to run as fast as
// possible.
Flag<bool> FLAG_throttle_cpu("throttle_cpu", true);

void ProcessArgs(int *argc, char **argv[]) {
  ParseFlags(argc, argv);
  if (*argc < 2) {
    PrintHelpAndExit((*argv)[0]);
  }
  args.filename = std::string((*argv)[1]);
}

std::string GetSaveFilename(const std::string &filename) {
  size_t i = filename.find_last_of(".");
  if (i == std::string::npos) return filename + kSaveExtension;
  return filename.substr(0, i) + kSaveExtension;
}

int main(int argc, char *argv[]) {
  ProcessArgs(&argc, &argv);
  std::string save_filename = GetSaveFilename(args.filename);
  INFOF("Save file: %s", save_filename.c_str());

  std::shared_ptr<EventDispatch> dispatch = dgb::GlobalDispatch();
  std::shared_ptr<Clock> clock(new Clock(*FLAG_throttle_cpu));
  std::shared_ptr<Interrupts> interrupts(new Interrupts());
  std::shared_ptr<Timers> timers(new Timers(clock, interrupts));
  std::shared_ptr<Input> input(new Input());
  std::shared_ptr<WindowController> window_controller(
      new WindowController(input, dispatch));
  window_controller->set_print_fps(*FLAG_print_fps);
  std::shared_ptr<Display> display(
      new Display(Display::kDisplayWidth*2, Display::kDisplayHeight*2,
                  *FLAG_debug_windows, clock, interrupts, window_controller));
  std::shared_ptr<Cartridge> cartridge(
      new Cartridge(args.filename, save_filename, clock));
  std::shared_ptr<SoundController> sound_controller(new SoundController());
  Memory memory(cartridge, display, input, timers, sound_controller);
  CPU cpu(clock, interrupts);

  dispatch->RegisterObserver(dgb::EVENT_START_DEBUGGER, [&cpu](const Event &event) {
    cpu.set_debug(true);
  });
  dispatch->RegisterObserver(dgb::EVENT_TOGGLE_PAUSE, [&cpu](const Event &event) {
    cpu.set_paused(!cpu.paused());
    if (cpu.paused()) {
      INFOF("Pause.");
    } else {
      INFOF("Unpause.");
    }
  });
  dispatch->RegisterObserver(dgb::EVENT_THROTTLE, [&clock](const Event &event) {
    clock->set_throttle(event.bool_value());
  });

  if ((*FLAG_bootloader).empty()) {
    cpu.set_pc(0x100);
  } else {
    memory.LoadBootloader(*FLAG_bootloader);
  }

  if (!(*FLAG_breakpoint).empty()) {
    // NOTE: the breakpoint is interpreted as a hex string
    cpu.set_breakpoint(std::stoi(*FLAG_breakpoint, 0, 16));
  }
  if (!(*FLAG_breakpoint_opcode).empty()) {
    // NOTE: the opcode is interpreted as a hex string
    int opcode = std::stoi(*FLAG_breakpoint_opcode, 0, 16);
    if (0 <= opcode && opcode <= 0xFF) {
      cpu.set_breakpoint_opcode(opcode);
    } else {
      ERRORF("Bad breakpoint opcode value: 0x%04x", opcode);
    }
  }

  cartridge->PrintCartridgeDebug();

  cpu.StartLoop(&memory);
  display->Loop();
  cpu.Kill();
  cpu.Wait();

  // Flush the RAM to the save file before exiting.
  cartridge->Persist();

	return 0;
}
