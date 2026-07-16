NovaGG Game Gear Emulator [README.md](https://github.com/user-attachments/files/30104416/README.md)# NovaGG

Sega Game Gear emulator core, C++20. Same overall structure and feature
set as the NovaGBA project (Core/CPU, Core/Memory, Core/GPU, Core/Sound,
Core/Input, Core/System), but a completely different CPU architecture
(Zilog Z80, 8-bit) and video chip (Sega VDP, tile-based Mode 4) - nothing
was reused code-for-code, only the project shape.

## What works (compiled and smoke-tested with g++ in this environment)

- **Z80 CPU**: main register set + shadow registers (AF'/BC'/DE'/HL'),
  IX/IY, full unprefixed opcode table, CB-prefixed (rotate/shift/BIT/RES/SET),
  ED-prefixed (16-bit ADC/SBC HL, block instructions LDIR/CPIR/INIR/OTIR
  and their variants, IM 0/1/2, LD A,I / LD A,R, RRD/RLD, NEG, RETN/RETI),
  and a real subset of DD/FD (IX/IY) - direct load/arithmetic/INC/DEC,
  (IX+d)/(IY+d) addressing for the LD and ALU blocks, DD CB/FD CB
  displacement bit ops. Interrupt modes IM0 (simplified to behave like
  IM1), IM1, IM2, plus NMI.
- **Bus**: Sega mapper ROM banking (3 switchable 16KB slots + the
  well-known "first 1KB always reads bank 0" boot-vector quirk), 8KB
  work RAM with mirroring, I/O port routing to VDP/PSG/Keypad.
- **VDP**: real control/data port protocol including the read-ahead
  buffer quirk, VRAM+CRAM storage, Mode 4 background tile rendering
  with horizontal/vertical scroll - tested end to end (hand-built
  tilemap + pattern + palette renders the correct color).
- **PSG** (SN76489-compatible): real latch/data byte register protocol
  for 3 tone channels + noise, Game Gear stereo panning register.
- **Keypad**: button ports + the Game Gear-specific Start button port.
- Save states, fast-forward, CPU tracer, FPS counter, audio fade-out -
  same designs as NovaGBA, all tested (see below).

## Known limitations - read before assuming a real ROM will boot

- **Sprites are not implemented** - only the background tile layer
  renders. Most real games use sprites for the player character and
  most action on screen, so this alone will keep real ROMs from looking
  right even if the CPU runs them correctly.
- **No real PSG audio output** - register writes are captured correctly,
  but there's no actual tone/noise synthesis or mixing yet.
- **DD/FD (IX/IY) coverage is a real subset, not exhaustive.** The most
  commonly generated forms are covered (loads, arithmetic, INC/DEC,
  displacement addressing, DD CB/FD CB bit ops), but not every
  documented or undocumented IX/IY opcode. Anything unimplemented falls
  through to executing the following byte as a plain unprefixed
  instruction (which is what real Z80 hardware does for genuinely
  undefined DD/FD-prefixed forms), rather than crashing - but that's
  only correct for the instructions that are actually undefined; a
  documented IX/IY opcode this doesn't recognize would silently misbehave
  instead of throwing. Worth double-checking if a real ROM does anything
  unusual with IX/IY.
- **IM0 is simplified to behave like IM1** (jump to 0x0038) rather than
  actually executing whatever instruction is on the data bus, which is
  what IM0 really means on real hardware. Game Gear software essentially
  always uses IM1, so this is a low-risk simplification but not
  hardware-accurate.
- **No PAL timing** - only NTSC (262 scanlines/frame) is implemented.
- **VDP extended 224/240-line modes aren't implemented** - only the
  standard 192-line-equivalent timing model.
- **No Windows .exe** - same as NovaGBA, no `mingw-w64` cross-compiler
  or network access in this sandbox to get one. Only a Linux binary was
  built and tested here.

## Build

```
make
make demo     # hand-assembled Z80 loop + VDP Mode 4 tile render smoke test
./NovaGG rom.gg
```
