# TI-99/4A GROM, GPL, Extended BASIC Interop, & SAMS/Super-Cart Architecture

Covers both **consuming built-in / Extended BASIC GROMs** and **authoring custom GROM / GPL / hybrid banked RAM+ROM+GROM cartridges**.

---

## 1. Real GROM Hardware Physics & Multi-Base Addressing (`0x9800` / `0x9C00`)

### 1.1 Chip Geometry: 6 KB Real Silicon vs. 8 KB Emulated/Flash Slots
The 64 KB GROM address space (`0x0000–0xFFFF`) is divided into **eight 8 KB GROM slots (`0..7`, top 3 bits `addr[15:13]`)**:
- **Slots `0, 1, 2` (`0x0000–0x5FFF`)**: Built-in **Console GROMs** (respond on *every* GROM base).
- **Slots `3, 4, 5, 6, 7` (`0x6000–0xFFFF`)**: **Cartridge GROMs** (up to 5 slots per base).
- **6 KB Silicon Quirk (`TMC0430`)**: Original TI GROM chips hold only **6 KB** (`0x0000–0x17FF` within each 8 KB slot); reading `0x1800–0x1FFF` on real TI silicon returns `GROM[off & 0x07FF] | GROM[(off & 0x07FF) + 0x1000]`. Modern FlashGROM99 / FinalGROM99 / UberGROM / FPGA (`icy99`) implement the **full 8 KB per slot** (`40 KB` per cartridge base).
- **13-Bit Page-Wrap Trap**: A GROM's internal auto-increment counter is **13 bits (`0x0000–0x1FFF`)**! Reading past the end of an 8 KB slot (`0x7FFF -> 0x6000`) **wraps back to the start of the SAME GROM chip**, NEVER crossing into `0x8000`! Always re-write `GROMWA` when crossing an `0x2000` boundary (or stay within 6 KB / 8 KB chunks).
- **Bus Speed Reality**: GROM is NOT CPU-bus memory—it is a slow byte-serial peripheral (~17–23 wait clocks per byte read on real hardware, ~3–4× slower than Cart ROM). **Never** execute tight inner loops in GPL when speed matters; use GROM as **zero-CPU-address-space bulk storage**.

### 1.2 Multi-Base GROMs (Up to 16 Bases = **640 KB** Without Using Any CPU Address Space!)
CPU address lines `A10..A13` (`0x0004` stride) select **16 independent Cartridge GROM Bases (`base = 0..15`)**:
- **Base `b` (`0..15`) Ports**:
  - Read Data: `0x9800 + 4*b` (`0x9800, 0x9804, ..., 0x983C`)
  - Read Addr: `0x9802 + 4*b` (returns `current_addr + 1`, MSB then LSB!)
  - Write Data (GRAM): `0x9C00 + 4*b`
  - Write Addr: `0x9C02 + 4*b` (**MSB first, LSB second!**)
- **Why Multi-Base GROM is an Exploitable "Super Support Cart" Superpower**:
  While CPU Cart ROM is squeezed into a single 8 KB window (`0x6000–0x7FFF`), Multi-Base GROM gives an assembly/C program **up to 640 KB (16 × 40 KB) of hardware-auto-incrementing asset storage** (graphics, tilemaps, LUTs, audio/speech streams, overlays) that can stream **directly into VDP VRAM (`MOVB *R_gromrd, *R_vdpwd`) in 1 word with zero RAM buffer**!

---

## 2. Built-In Console GROMs (`0..2`), Standalone `GPLLNK`, & Extended BASIC

### 2.1 Useful Assets & Routines Inside Console GROM 0 (`0x0000–0x1FFF`)
- **Direct Raw Read (Fastest — NO `GPLLNK` needed, works with `LIMI 0`!)**:
  - `0x06B4`: Standard uppercase + punctuation font (`ASCII 32..95`, 64 chars × **7 bytes**; prepend `0x00` for 8×8 VDP tiles).
  - `0x04B4`: Large title-screen font (`64` chars × 7 bytes).
  - *(For 99/4A true lowercase `ASCII 96..126` with descenders, GROM routine `0x004A` builds them dynamically, or read their 7-byte bitmaps from GROM 0 around `0x087A`!)*
- **Console GPL Subroutines (Invoked via `GPLLNK`)**:
  - `0x0016`: Load standard character set (`FAC` `0x834A` = VRAM pattern base).
  - `0x0018`: Load small-capitals character set (`FAC` `0x834A` = VRAM pattern base).
  - `0x004A`: Load lowercase character set with descenders (TI-99/4A only).
  - `0x0034` / `0x0036`: Play console Accept beep / Bad-Response honk.

### 2.2 Standalone Universal `GPLLNK` (Without Editor/Assembler Cart!)
`BLWP @0x6018` in old books **only exists when the Editor/Assembler cartridge occupies `0x6000`!** To call Console GROM routines from your **own** cartridge or standalone RAM program:
1. Console ROM **`0x0060`** loads the target GROM address from **`GPLWS R6` (`0x83EC`)** into `GROMWA`, clears status flags, pulses `LIMI 2; LIMI 0` at `0x0070`, and interprets GPL bytecodes until a GPL `RTN` (`0x00`) pops a return GROM address from the **GPL Subroutine Stack (`*0x8373`)**.
2. Universal `GPLLNK` (~90 bytes in RAM) pushes a pointer to a Console GROM `XML` instruction onto `@0x8373`, points that XML vector in PAD back to an assembly `LWPI / RTWP` stub, puts the target GROM routine address into `@0x83EC`, and executes `LWPI 0x83E0; B @0x0060`. *(Prefer direct GROM reads `@0x9800` whenever you just want font/table data!)*

### 2.3 Running Assembly Under TI BASIC / Extended BASIC (`CALL INIT` / `CALL LINK`)
Extended BASIC (`XB`) uses GROMs `3..6` (`24 KB` at GROM `0x6000–0xBFFF`) + two banked 8 KB CPU ROMs at `0x6000–0x7FFF`:
1. **MANDATORY `+0x60` (`+96`) SCREEN BIAS LAW (Hardware-Verified — E/A Manual §24.3, Console GROM 0)**:
   In TI BASIC and Extended BASIC, `VR4 = 0x00` sets the Pattern Table base to VRAM `0x0000`, which **overlaps** the 768-byte Screen Name Table (`0x0000–0x02FF` = pattern slots `0..95`) and Color/Sprite tables (`0x0300–0x03FF` = slots `96..127`)! Therefore TI BASIC & XB load the font for `ASCII 0x20..0x7F` at VRAM `0x0400–0x06FF` (**pattern slots `0x80..0xDF`**), shifting **every** screen byte by **`+0x60` (`+96`)**:
   - `' '` (`0x20`) on screen **MUST** be written as **`0x80`** (`LI Rx, 0x8000` for high-byte `MOVB`)!
   - `'0'` (`0x30`) -> **`0x90`**; `'A'` (`0x41`) -> **`0xA1`** (`AI Rx, 0x6000` on any high-byte ASCII register before `MOVB Rx, @0x8C00`!). Writing raw unshifted ASCII (`0x20`/`0x41`) displays garbage bytes from the Screen Name Table itself!
2. **Memory Ownership**: XB claims **all of High Expansion RAM (`0xA000–0xFFFF`)** for BASIC program/numeric variables! Assembly loaded via `CALL INIT` + `CALL LOAD` lives **only in Low Expansion RAM (`0x24F4–0x3FFF`, ~6.9 KB)** (`0x2000–0x24F3` holds XB's own support vectors, which are at *different* addresses from E/A's `0x20xx` vectors—either `REF` symbols via the tagging loader or hit hardware ports `0x8C00`/`0x8C02` directly!).
3. **Scratchpad Limits if Returning to BASIC/XB**: Only `0x8300–0x8317` (or `0x8300–0x830F` when passing `CALL LINK` parameters via `FAC` `0x834A`) is safe to clobber unless you take over the machine permanently.

---

## 3. Authoring GROM / GPL Cartridges & Hybrid "Super Support" Designs

### 3.1 GROM Cartridge Header (`GROM >6000`, Assembled with `xga99.py` or Raw `.byte`)
A GROM header at GROM `0x6000` uses the **exact same 16-byte `0xAA` layout** as a CPU ROM header, except `prog_list`'s `START` pointer is a **GROM address (`0x60xx`)** interpreted as **GPL bytecode**:
```text
        GROM >6000
        AORG >0000
        DATA >AA01, >0000, >0000, MENU, >0000, >0000, >0000, >0000
MENU    DATA >0000, START
        BYTE 4
        TEXT 'DEMO'
        EVEN
```

### 3.2 Escaping From GPL Into Native TMS9900 Assembly (`XML` Dispatch @ Console ROM `0x0608`)
In Console ROM (`0x0608–0x061C`), GPL instruction `XML >XY` (`0x0F, 0xXY`) computes `R4 = *(XML_Table[X] + 2*Y)` (Master Table at `0x0CFA`) and executes **`BL *R4`** (with `WP = 0x83E0`; `LWPI` takes over the machine, or `B *R11` returns to GPL!):
- **`XML >F0..>FF` (`X = 15`, base `0x8300`) — Zero-Cart-ROM RAM Escape!**:
  Copies native TMS9900 machine code from GROM into Expansion RAM and jumps via `@0x8300`:
  ```text
  * In xga99.py GPL:
  START   MOVE N_BYTES, G@PAYLOAD, @>A000  * Copy N_BYTES from GROM PAYLOAD to CPU RAM >A000
          DST  >A000, @>8300               * Store target CPU address 0xA000 at PAD 0x8300
          XML  >F0                         * Console ROM loads *(0x8300) -> BL @0xA000!
  ```
  - **Raw GPL Bytecode Template** (if emitting a GROM image directly from `.S` without `xga99.py`! Note GPL encodes CPU RAM `@addr` outside PAD as `0x8F, hi(addr - 0x8300), lo(addr - 0x8300)`, so `@0xA000` = `0x8F, 0x1D, 0x00`!):
    ```asm
    ; MOVE N, G@payload, @0xA000  ->  .byte 0x31, hi(N), lo(N), 0x8F, 0x1D, 0x00, hi(G_payload), lo(G_payload)
    ; DST 0xA000, @0x8300         ->  .byte 0xBF, 0x00, 0xA0, 0x00
    ; XML 0xF0                    ->  .byte 0x0F, 0xF0
    ```
- **`XML >70..>7F` (`X = 7`, base `0x6010`) — Hybrid GROM + Cart ROM Escape**:
  Dispatches through a 16-word function-pointer table at CPU Cart ROM **`0x6010–0x602E`** (`*(0x6010 + 2*Y)`). *(Similarly: `XML >80..>8F` -> `0x6030`, `XML >20..>2F` -> Low RAM `0x2000`, `XML >B0..>EF` -> High RAM `0xA000..0xD000`.)*

### 3.3 Pairing With SAMS Banked RAM (`1 MB` 74LS612 Mapper, CRU `0x1E00`)
For a "Super Support Cartridge" + Banked RAM system (`SAMS` / `icy99` `pager612`):
- **CRU Base `0x1E00` (`LI R12, 0x1E00`)**:
  - **`SBO 0`**: Exposes the sixteen 4 KB page registers at CPU **`0x4000–0x401E`** (`reg_addr = 0x4000 + 2*(cpu_addr >> 12)`: `0x4004, 0x4006` map `0x2000, 0x3000`; **`0x4014, 0x4016, ..., 0x401E`** map **`0xA000, 0xB000, ..., 0xF000`**!).
  - **`SBZ 0`**: Hides `0x4000–0x401E` registers (**always `SBZ 0`** after programming registers!).
  - **`SBO 1` / `SBZ 1`**: Enables (`SBO 1`) or disables (`SBZ 1`, restoring default pass-through pages `0x00..0x0F`) SAMS address translation!
- **CRITICAL HIGH-BYTE REGISTER LATCH RULE**:
  SAMS hardware latches the 8-bit physical page number (`0x00..0xFF` for 1 MB) from the **HIGH byte (`bits 0..7`, `value >> 8`)** written to `@0x4000 + 2*win`! (On the 8-bit PEB multiplexer, `MOV` writes low byte to odd `0x4001` first, then **overwrites with high byte at even `0x4000` second**; on FPGA `pager612`, `dbus_in = db_out[15:8]`.)
  ```asm
  ; Map physical 4 KB page in R0 (0x00PP, low byte) into CPU window 0xA000 (win = 10 -> 0x4014):
      LI   R12, 0x1E00        ; SAMS CRU base
      SBO  0                  ; Expose 0x4000..0x401E mapper registers
      SWPB R0                 ; Move page PP into HIGH byte (0xPP00)! (Or MOVB high byte)
      MOV  R0, @0x4014        ; Window 10 (0xA000..0xAFFF) = physical page PP
      SBZ  0                  ; Hide mapper registers
      SBO  1                  ; Enable SAMS mapping! (SBZ 1 restores pass-through)
  ```
- **Ultimate Hybrid Architecture**:
  - **Cart ROM (`0x6000`, banked `378`)**: Zero-RAM-cost hot C/ASM code banks.
  - **Multi-Base GROM (`0x9800..0x983C`)**: Up to 640 KB of streaming assets / overlays (`GROM -> VRAM` in 1 instruction).
  - **Scratchpad PAD (`0x8300–0x836E`)**: `WP = 0x8300` + 78 bytes of 16-bit 0-wait inner loops & bank trampolines.
  - **32K / SAMS RAM (`0x2000`, `0xA000`)**: Mutable framebuffers, decompress buffers, FreeRTOS tasks, and C++/Rust heaps.
