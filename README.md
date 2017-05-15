# Game Boy Music Engine
## (No Catchy Name, Sorry)

### What Is It?

The music engine for a Game Boy game I'm working on.

I'm putting the code up here for free (public domain, no warranty from me, I'm not liable for any harm it causes, etc.) use in other people's Game Boy
homebrews. Any feedback or bug fixes would be much appreciated, of course.

Please note that it's fairly ill-tested, since I can't be bothered to write a decent song in Famitracker to put it through any kind of paces. So, if I
were you, I wouldn't depend on it for any mission-critical work just yet, or ever. Unless you want to do plenty of debugging on your own, you might
want to hold off on using this seriously until *I'm* using it seriously.

### Features

- FamiTracker converter translates from FamiTracker (text) modules to files suited for inclusion in your game
- Compressed pattern data
- Volume, pitch, hipitch, and duty sequences
- Pretty fast, hopefully

### Missing Features

- No support for arpeggio instrument sequences
- No support for the vast majority of effects
- **Requires** that all frames use the same pattern number for each channel
- **Requires** 60Hz engine speed
- **Requires** that all sequences for a given instrument "line up" (are of the same length and loop in the same way)
- Obviously, the FamiTracker converter rejects things the Game Boy simply can't play, like extra wave channels, the triangle channel, etc.
- No support for sound effects yet
- A bunch of other stuff

The missing features list is subject to change based on my needs.

### How Do I Include It In My Game?

The engine is written in RGBDS-compatible assembly; you'll want a recent version from
the GitHub repository. The entire engine is in src/gbsound.asm, except for the decompressor,
which is in src/decompress.asm. Assemble these two files and link them in.

To play a song, first call InitSndEngine with HL pointing to your song data. Then, each frame,
call UpdateSndFrame to drive playback. The example src/main.asm program should illustrate fairly well
how you might use it in a real program.

### Converting from FamiTracker

One way to generate song data is with the included FamiTracker converter. It's written in
C++11, so you'll need a relatively modern g++ to build it; otherwise, it has no dependencies - just
run make to build it. The resulting famiconv program takes two arguments, an input FamiTracker
text module (use File->Export text... in FamiTracker) and an output file. The output file can
then be included in your program using RGBDS' INCBIN directive.

### Caution

As written, this music engine is *not* MBC banking aware - it assumes it's running from
the HOME bank and that it has access to all of the data it needs. It also spreads itself out
quite luxuriously across the entire lower 4KB bank of RAM - it doesn't use anywhere close to
every last byte, but it does take all the primo (page-aligned) addresses. This isn't how I
intend to use it in production, but everyone's ROM, RAM, and banking needs are different, so
I leave it to you to sort that sort of thing out.

### Song format

```
1 byte - initial value for $FF24 (aka NR50)
1 byte - initial value for $FF25 (aka NR51)
1 byte - tempo control byte
1 byte - byte length of waves
  for each:
    16 bytes of sample data (0-255, where 0 => 256)
1 byte - byte length of pattern table (0-255, where 0 => 256)
  for each:
      2 byte offset from song start to pattern data
1 byte - byte length of instrument table (0-255, where 0 => 256)
  for each:
      2 byte offset from song start to instrument data

m bytes - instrument code
n bytes - compressed pattern code
```

_TODO: document the instrument code, pattern code_
