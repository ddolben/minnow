# Interesting Stuff

This file will attempt to accumulate the interesting things that I've found or
discovered throughout the development of the Minnow emulator.

## Interesting Bugs

### Horizontal Flipping Sprites (Super Mario Land)

TODO

### Top Scanline Glitch (Super Mario Land)

At this point, I had the emulator reliably running the first few levels of the
Super Mario Land ROM with one exception: when Mario is walking to the right,
occasionally the top few scanlines will glitch as if they're momentarily
shifting to the left.

![Scanline Glitch](scanline_glitch.png)

Now I already know that Super Mario Land uses mid-frame scanline shifting to
adjust the background scenery (that produced some interesting visuals before I
implemented the LCD coincidence interrupt), so I figured it was a result of the
background scroll x value not being set properly.

I traced the game code that set the scroll x value to two different routines,
ultimately resulting from the LCD STAT interrupt handler (coincidence interrupt
triggered when LCDCY == 15) and the VBLANK interrupt. I decided to check when
the scanline Y=0 was being rendered with Scroll-X != 0, and voila, it
consistently happened within the code that waits for the [DMA transfer
function](http://bgb.bircd.org/pandocs.htm#lcdoamdmatransfers) to complete,
which is in the VBLANK handler (but before Scroll-X is set to 0).

At this point, I noticed that the VBLANK interrupt handler was taking more
cycles to complete than the VBLANK period (4560 cycles), which is why the top
scanline was being moved. Actually, since the top scanline of gameplay is
blank, it's the Y=1 scanline that we care about, but it was still taking long
enough that we would be in Y=1 before the VBLANK handler completed.

It was taking on the order of 10's of cycles longer than the start of the Y=1
scanline. I loaded up my nitty-gritty [GameBoy LCD timing reference](
http://gameboy.mongenel.com/dmg/gbc_lcdc_timing.txt)
and, sure enough, the scanline data transfer doesn't actually start as soon as
the LCD controller reaches the next scanline. There's a brief period in which
the LCD controller searches the OAM table before beginning the data transfer,
and it takes between 77 and 83 cycles to complete. So, I added an 83-cycle
delay before rendering the scanline out to the framebuffer. It _mostly_ removed
the glitch, but it would still happen once in a while.

At this point, I was stumped. I loaded up the Super Mario Land ROM
into a fully working emulator that I'd downloaded from online and tried it.
**The glitch was there**. I figured it was pretty unlikely that we'd both made
the exact same timing mistake, so I dug out my old GameBoy (you know, the
see-through purple GameBoy color that everyone had in the 90's?) and tried it
out. **The glitch was present on the cartridge version on real GameBoy
hardware!**

So, it turns out that the developers of Super Mario Land cartridge were cutting
their VBLANK handler very close to the number of available cycles, and
occasionally would use up too many, causing a small glitch in the top line of
the screen. Since that line was very close to the bezel on the edge of the
screen and most people weren't intently staring at it, they got away with it.

It's pretty amazing the things that game developers did to squeeze more
performance out of hardware on older consoles. It's humbling to think that,
while I sit here struggling to write stuff in C++ for moderm CPUs, they built
that game on a 4 MHz processor with a finicky LCD controller, all without
modern development tools. Hats off to you, Nintendo.
