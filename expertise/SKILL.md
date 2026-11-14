---
name: tms9900-ti99-mastery
description: Grandmaster TMS9900 assembly optimization and TI-99/4A hardware programming (VDP TMS9918A, Scratchpad PAD, GROM/GPL, CRU/TMS9901, SN76489 sound, Cartridges/EA5, and LLVM C/ASM interop). Load whenever writing, debugging, or optimizing TMS9900 assembly or TI-99/4A software.
---

# TMS9900 & TI-99/4A Agent Skill Router

Instead of loading ~380K tokens of raw TI manuals, load ONLY the compact, surprise-weighted artifacts needed for the task (~4.6K–7K tokens total):

## 1. Always Load First
- **[`CORE_CONSTITUTION.md`](CORE_CONSTITUTION.md)** (~4.6K tokens): Mandatory for **all** TMS9900 assembly or compiler work. Covers `clang --target=tms9900` `.S` syntax laws (`;` comments, `0x` hex), `MOVB` high-byte law, `C A, B` left-to-right jump polarity vs `S A, B` right-minus-left & `JOC`=no-borrow polarity, exact `ST` flag preservation matrix, `R0` non-indexing & variable-shift rules, `C + W·M` cycle table, C ABI, and core 16/32/64-bit ASM idioms.

## 2. Load Task-Relevant Domain Cards (`cards/`) On Demand
- **[`cards/TI99_MEMORY_PAD_AND_ROM.md`](cards/TI99_MEMORY_PAD_AND_ROM.md)**: Memory map (`0x0000–0xFFFF`), 16-bit zero-wait Scratchpad PAD (`0x8300–0x83FF`) layout & `0x8320` fast-code trampolines, opposite GROM (MSB-first) vs VDP (LSB-first) address order, Console ROM `KSCAN` (`0x000E`), VBlank `0x8379`/`0x837B`, `0x83D4` `VR1` shadow, and GROM font loading.
- **[`cards/TMS9918A_VDP_AND_GRAPHICS.md`](cards/TMS9918A_VDP_AND_GRAPHICS.md)**: TMS9918A VDP ports (`0x8C00`/`0x8C02`/`0x8800`/`0x8802`), Graphics I / Graphics II bitmap / Multicolor / Text register setups, Sprite Attribute Table quirks (`Y - 1` scanline offset, `0xD0` terminator, `0x80` Early Clock left-edge entry), and unrolled `*R+ -> *R_vdpwd` blitters.
- **[`cards/CRU_TMS9901_SOUND_SPEECH.md`](cards/CRU_TMS9901_SOUND_SPEECH.md)**: CRU `R12 = 2 * bit` addressing, `LDCR`/`STCR` 1..8 high-byte vs 9..16 word transfer trap, `TB` (`JEQ` = bit is 1!), raw TMS9901 keyboard/joystick scanning, and SN76489 `0x8400` 3-byte tone/noise/attenuation protocol.
- **[`cards/CART_EA5_BANKING_AND_ABI.md`](cards/CART_EA5_BANKING_AND_ABI.md)**: `0x6000` `0xAA` cartridge ROM headers, `378`-style `CLR @0x6000+2*bank` ROM banking, 6-byte EA5 executable headers, `BLWP`/`RTWP` zero-copy coroutines & `*R14+` inline-`DATA` linkage, and multi-word C↔ASM stack ABI.
- **[`cards/GROM_GPL_XB_AND_SAMS.md`](cards/GROM_GPL_XB_AND_SAMS.md)**: Real GROM hardware physics (`TMC0430` 6KB vs 8KB slots, 13-bit auto-increment page wrap, 16 GROM bases = 640 KB zero-CPU-address-space streaming), Console GROM 0 assets & standalone `GPLLNK`, TI BASIC / Extended BASIC `+0x60` screen bias & Low-RAM `0x24F4` rules, GPL `XML >F0`/`>70` native escape (`0x0CFA`), and 1 MB SAMS (`CRU 0x1E00`) hybrid architecture.

## 3. Gold-Standard Exemplars (`exemplars/`)
Consult [`exemplars/`](exemplars/) for cycle-counted, simulator-verified reference implementations of 32/64-bit math, dual-stream memory/RLE/LZ kernels, bit-parallel stencils, PAD-resident VDP blitters, `BLWP` coroutines, and banked cartridges.

## 4. Closed-Loop Verification & Cycle Profiling
Always assemble and verify generated code with the LLVM toolchain and `tms9900-trace`:
```bash
# Assemble & link
llvm-project/build/bin/clang --target=tms9900 -c kernel.S -o kernel.o

# Execute & measure exact cycles + TI-99 VDP/GROM/PSG peripheral state
../tms9900-trace/build/tms9900-trace --ti99-io -l 0x2000 -e 0x2000 -w 0x8300 -n 1000000 -S test.bin
```
