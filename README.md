# Minnow

Minnow is a work-in-progress emulator for the Game Boy platform. It's currently
under development, and I make no guarantees that it will work correctly, or
work at all.

It has only been tested on the following platforms:

* OS X Yosemite (clang++ 6.1.0)

# Dependencies

Minnow builds under C++ 11 and depends on the following:

* GNU make
* clang++ (or g++, if you dare)
* [SDL 2.0](https://www.libsdl.org)

# Interesting Stuff

Check out [this page](notes/interesting_stuff.md) for some interesting stuff
I've discovered along the way.

# References

* http://gigi.nullneuron.net/gigilabs/sdl2-pixel-drawing/
* https://cturt.github.io/cinoop.html
* [RealBoy blog series](https://realboyemulator.wordpress.com/2013/01/03/a-look-at-the-game-boy-bootstrap-let-the-fun-begin/)
* [Opcode Cheat Sheet](http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html)
* http://www.devrs.com/gb/files/opcodes.html
* [GameBoy Dev Book](http://bgb.bircd.org/pandocs.htm)
* [GameBoy LCD Cycle Timing](http://gameboy.mongenel.com/dmg/gbc_lcdc_timing.txt)
* [GameBoy CPU Timing](http://hitmen.c02.at/files/releases/gbc/gbc_cpu_timing.txt)
* [GameBoy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)
  + NOTE: this manual has some errors (e.g. its description of the carry flags is wrong)
