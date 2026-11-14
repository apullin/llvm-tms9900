# TI-99/4A Memory Map, Scratchpad PAD (`0x8300`), GROM & Console ROM

---

## 1. Complete 64KB CPU Address Space & Bus Wait-State Physics

| Address Range | Region | Bus Width & Wait States | Critical Rules |
|---|---|---|---|
| `0x0000–0x1FFF` | Console ROM (8 KB) | **16-bit, 0 wait states** | Interrupt vectors, GPL interpreter, `KSCAN`, floating-point (`XMLLNK`). |
| `0x2000–0x3FFF` | Low Expansion RAM (8 KB) | 8-bit mux (**+4 wait/word**) | Present only with 32K expansion. E/A utilities live at `0x2000–0x27FF` when E/A cart used. |
| `0x4000–0x5FFF` | Peripheral DSR ROM (8 KB) | 8-bit mux (+4 wait/word) | Banked via CRU bit `0` at device CRU base (`0x1100` disk, `0x1300` RS232, etc.). |
| `0x6000–0x7FFF` | Cartridge ROM (8 KB) | 8-bit mux (**+4 wait/word**) | Starts with `0xAA` GROM/ROM header. Writing to `0x6000/0x6002/...` switches 8KB ROM banks! |
| `0x8000–0x83FF` | **Scratchpad PAD RAM** (256 B mirrored; canonical **`0x8300–0x83FF`**) | **16-bit, 0 wait states!** | **ONLY zero-wait RAM in the machine!** Always put active `WP` (`0x8300`) and hot inner loops (`0x8320–0x836E`) here! |
| `0x8400–0x87FF` | Sound Chip Write (`SOUND = 0x8400`) | 8-bit mux (+4 wait + audio hold) | Byte write ONLY (`MOVB`). High byte of register goes to SN76489/TMS9919. |
| `0x8800–0x8BFF` | VDP Read: `VDPRD = 0x8800`, `VDPSTA = 0x8802` | 8-bit mux | Byte read ONLY (`MOVB @0x8800, Rx`). Reading clears VDP address latch! |
| `0x8C00–0x8FFF` | VDP Write: `VDPWD = 0x8C00`, `VDPWA = 0x8C02` | 8-bit mux | Byte write ONLY (`MOVB Rx, @0x8C00`). **VDP address is LSB first, MSB second!** |
| `0x9000–0x93FF` | Speech Read (`SPCHRD = 0x9000`) | 8-bit mux | TMS5220 Speech Synthesizer read. |
| `0x9400–0x97FF` | Speech Write (`SPCHWT = 0x9400`) | 8-bit mux | TMS5220 Speech Synthesizer write. |
| `0x9800–0x9BFF` | GROM Read: `GROMRD = 0x9800`, `GROMRA = 0x9802` | 8-bit mux (slow GROM hold) | Byte read ONLY (`MOVB @0x9800, Rx`). Auto-increments internal 13-bit GROM counter! |
| `0x9C00–0x9FFF` | GROM Write: `GROMWD = 0x9C00`, `GROMWA = 0x9C02` | 8-bit mux (slow GROM hold) | Byte write ONLY (`MOVB Rx, @0x9C02`). **GROM address is MSB first, LSB second!** |
| `0xA000–0xFFFF` | High Expansion RAM (24 KB) | 8-bit mux (**+4 wait/word**) | Main code/data/heap/stack (`SP = 0xFFFC`) when 32K RAM expansion is present. |

### Why TI Split Read (`0x8800`, `0x9800`) and Write (`0x8C00`, `0x9C00`) Ports by `0x0400`
The TMS9900 hardware **always performs a dummy read of the destination before writing** on `MOV` and `MOVB`. Putting write ports in a separate decode block (`0x8C00`, `0x9C00`) ensures the dummy read hits a write-only address rather than triggering a destructive `VDPRD`/`VDPSTA`/`GROMRD` read!
- **NEVER use word `MOV` on MMIO ports (`0x8400–0x9FFF`)**: the 16-to-8-bit bus multiplexer writes the **low byte to `addr | 1` first** and the **high byte to `addr` second**, sending two bytes in scrambled order! Always use **`MOVB`**.

---

## 2. GROM vs. VDP Address Endianness Trap!

**VDP (`0x8C02`) and GROM (`0x9C02`) take 16-bit addresses in OPPOSITE byte orders:**

- **GROM (`0x9C02`) — HIGH BYTE (MSB) FIRST, LOW BYTE (LSB) SECOND**:
  ```asm
  ; Set GROM address in R0 (e.g. 0x06B4 = Console uppercase ASCII 32..95 7-byte font!)
  MOVB R0, @0x9C02        ; 1st write: MSB (high byte of R0)
  SWPB R0
  MOVB R0, @0x9C02        ; 2nd write: LSB
  SWPB R0                 ; optional: restore R0
  ; Now each MOVB @0x9800, Rx reads 1 byte and auto-increments GROM address!
  ```
  - Console GROM 0 font tables (free 96-char / 64-char fonts in every TI-99/4A without ROM bloat!):
    - `0x06B4`: Standard uppercase + symbols (`ASCII 32..95`, **7 bytes per glyph** — prepend 1 `0x00` byte for 8×8 VDP tiles!).
    - `0x04B4`: Title-screen large font.

- **VDP (`0x8C02`) — LOW BYTE (LSB) FIRST, `(MSB & 0x3F) | 0x40` SECOND**:
  ```asm
  ; Set VDP write address in R0 (0x0000..0x3FFF) without clobbering R0 permanently:
  SWPB R0
  MOVB R0, @0x8C02        ; 1st write: LSB (now in high byte of R0!)
  SWPB R0
  ORI  R0, 0x4000         ; set write flag (0x40 in high byte; omit ORI for VRAM read!)
  MOVB R0, @0x8C02        ; 2nd write: (MSB & 0x3F) | 0x40
  ```

---

## 3. Exact Scratchpad PAD Map (`0x8300–0x83FF`) & PAD Code Trampolines

In standalone C/ASM programs (`LIMI 0`), `WP = 0x8300` uses **`0x8300–0x831F`** (`R0–R15`).
- **Register byte-aliasing trick**: When `WP = 0x8300`, register `Rn` lives at `0x8300 + 2*n` (high byte) and **`0x8300 + 2*n + 1` (low byte)**! E.g. `MOVB @0x8301, @0x8C00` writes `R0`'s low byte straight to VDP without `SWPB`!
- **`0x8320–0x836E` (78 bytes) — SAFE SCRATCHPAD ZONE FOR FAST INNER LOOPS**:
  Copying a relocatable inner loop (`MOV *R2+, *R3+` word loop) to **`0x8320`** and calling `BL @0x8320` (or jumping `B @0x8320` when the loop ends with `B *R11`!) runs instruction fetches from 16-bit 0-wait RAM instead of 8-bit Cart ROM.
  - **Relocatability rule**: PC-relative jumps (`JMP`, `JNE`, `JEQ`, `JLT`, ...) inside the copied block ARE position-independent! Absolute `B @sym` / `BL @sym` target fixed addresses; `B *R11` returns directly to caller!
- **`0x8370–0x83FF` — CONSOLE SYSTEM AREA (DO NOT CLOBBER IF USING `KSCAN`, `LIMI 2`, OR `GPLLNK`!)**:
  - `0x8370–0x8371`: Highest free VDP RAM address
  - `0x8372 / 0x8373`: GPL data stack / **GPL subroutine stack pointer** (`0x8373`)
  - `0x8374`: `KSCAN` keyboard mode (`0` = full kbd, `1` = joystick 1 / left kbd, `2` = joystick 2, `5` = standard BASIC kbd)
  - `0x8375`: `KSCAN` returned ASCII keycode (`0xFF` if no key pressed!)
  - `0x8376 / 0x8377`: Joystick Y / X (`-4`, `0`, or `+4`)
  - `0x8379`: **VDP 60Hz interrupt frame counter** (incremented by Console ISR on every VBlank)
  - `0x837A`: Max auto-motion sprites (`CLR` / `0x00` to stop Console ISR from moving sprites!)
  - `0x837B`: **Console ISR copy of VDP Status Register** (bit `0x80` frame, `0x40` 5th sprite, `0x20` sprite collision)
  - `0x837C`: **GPL Status byte** (bit `0x20` = new key pressed in `KSCAN`)
  - `0x83C0–0x83DF`: **Console ISR Workspace (`ISRWS`)**:
    - `0x83C2` (`ISRWS R1` high byte): **ISR Control Flags** (`0x80` = disable ALL auto-sprite/sound/QUIT processing; `0x40` = skip sprite motion; `0x20` = skip sound list; `0x10` = skip `FCTN+=` reset). Always set `LI Rx, 0x8000; MOVB Rx, @0x83C2` in standalone games using `LIMI 2`!
    - `0x83C4` (`ISRWS R2`): **User VBlank ISR Hook Address** (`0x0000` = none).
    - **`0x83D4`** (`ISRWS R10` high byte): **BIOS VDP Register 1 (`VR1`) Shadow Byte** — ALWAYS write your `VR1` byte to `@0x83D4` BEFORE/WITH setting `VR1`, or VBlank/`KSCAN` will overwrite `VR1`!
    - `0x83D6` (`ISRWS R11`): Screen-blank timeout word (`CLR @0x83D6` prevents blanking).
  - `0x83E0–0x83FF`: **GPL Workspace (`GPLWS`)** (`R13 = 0x9800`, `R14 = 0x01xx`, `R15 = 0x8C02`).

### User VBlank ISR Hook (`@0x83C4`) — Exact Console ROM (`0x0AA8–0x0ABE`) Contract!
1. Console ROM enters `@0x83C4` via **`BL *R12` with `WP = 0x83E0` (`GPLWS`)** — **NOT** `ISRWS` (`0x83C0`) and **NOT** foreground `WP = 0x8300`!
2. **Return via `B *R11` in `GPLWS` (`0x83E0`) — NEVER `RTWP`!** (Console ROM executes `LWPI 0x83C0; RTWP` right after your hook returns.)
3. **SACRED REGISTERS**: Inside `@0x83C4`, **NEVER clobber `GPLWS R11` (return addr) or `R13–R15` (`0x9800, 0x01xx, 0x8C02`)** (next VBlank reads `VDPSTA` via `@-0x0400(R15)` and increments `@0x8379` via `AB R14, @0x8379`!), and **NEVER switch to foreground `WP = 0x8300`**! Either use `GPLWS R0..R10` or switch to a private 32-byte ISR workspace (`LWPI isr_ws ... LWPI 0x83E0; B *R11` or `BLWP`).
4. **Foreground VDP Latch Safety**: Keep `LIMI 0` during foreground VDP/GROM accesses and pulse `LIMI 2; LIMI 0` at frame-wait sync points so VBlank never fires mid-`VDPWA` latch!

### Calling Console ROM `KSCAN` (`0x000E`) Without Editor/Assembler Cart
```asm
; Scans keyboard using Console ROM directly (preserves caller WP = 0x8300):
    LWPI 0x83E0
    MOV  R11, @0x8300       ; save GPLWS R11 in scratch R0 of 0x8300
    BL   @0x000E            ; Console ROM KSCAN entrypoint
    MOV  @0x8300, R11       ; restore GPLWS R11
    LWPI 0x8300
    MOVB @0x8375, R0        ; high byte = ASCII key (0xFF if none)
```
*(In Console ROM, `0x0060` loads GROM address from `GPLWS R6` (`0x83EC`) and enters the GPL bytecode interpreter at `0x0070`; Universal `GPLLNK` pushes a return vector onto the GPL stack `@0x8373` and branches to `0x0060`.)*
