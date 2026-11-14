# TMS9918A VDP & TI-99/4A Graphics Mastery Card

---

## 1. VDP Port Protocol (`VDPWD = 0x8C00`, `VDPWA = 0x8C02`, `VDPRD = 0x8800`, `VDPSTA = 0x8802`)

All VDP ports are **8-bit** (`MOVB` transfers the **HIGH byte** of a TMS9900 register!).

### 1.1 Write VDP Register `VRn` (`n = 0..7`) with `val` (`0x00..0xFF`)
1st byte to `0x8C02`: **`val`**; 2nd byte to `0x8C02`: **`0x80 | n`**.
- **CRITICAL TI-99 RULE FOR `VR1`**: Whenever modifying VDP Register 1 (`n == 1`), **ALWAYS mirror `val` to `@0x83D4`** (`MOVB val_hi, @0x83D4`) so the Console ISR / `KSCAN` never overwrites `VR1`!
```asm
; R0 low byte = val, R1 low byte = reg (0..7)
    SWPB R0
    MOVB R0, @0x8C02        ; 1st write: register value
    ORI  R1, 0x0080
    SWPB R1
    MOVB R1, @0x8C02        ; 2nd write: 0x80 | reg
```

### 1.2 Set VRAM Write Address (`0x0000..0x3FFF`) & High-Speed Streaming
1st byte to `0x8C02`: **`addr & 0xFF` (LSB)**; 2nd byte to `0x8C02`: **`((addr >> 8) & 0x3F) | 0x40`**.
```asm
; R0 = vram_addr (0x0000..0x3FFF), R1 = src, R2 = len (>0)
    LI   R4, 0x8C00         ; Hold VDPWD in a register so *R4 is 2B (vs @0x8C00 = 4B)!
    SWPB R0
    MOVB R0, @0x8C02        ; LSB first
    SWPB R0
    ANDI R0, 0x3FFF
    ORI  R0, 0x4000         ; write-mode bit (0x40 in high byte)
    MOVB R0, @0x8C02        ; MSB | 0x40 second
    ; Fast 8x unrolled byte stream (2B / 24c per byte — vs 4B / 28c for @0x8C00!):
    MOV  R2, R3
    SRL  R3, 3
    JEQ  .Lrem
.Lblk8:
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    MOVB *R1+, *R4
    DEC  R3
    JNE  .Lblk8
.Lrem:
    ANDI R2, 7
    JEQ  .Ldone
.Ltail:
    MOVB *R1+, *R4
    DEC  R2
    JNE  .Ltail
.Ldone:
```
*(For VRAM **Reads**: omit `ORI 0x4000` so bits `7:6` of the 2nd byte are `00`; the VDP immediately prefetches `VRAM[addr]` into its read-ahead buffer, and each `MOVB @0x8800, Rx` returns the prefetched byte and advances `addr`!)*
- **CRITICAL VDP READ-MODIFY-WRITE (PIXEL PLOTTING) LAW**:
  Reading `VDPRD` (`@0x8800`) **auto-increments the VDP's internal 14-bit address counter by `+1`** AND leaves VDP in read mode! Writing `@0x8C00` immediately after `@0x8800` corrupts **`VRAM[addr + 1]`**! You **MUST re-latch the write address (`0x4000 | addr`) at `@0x8C02`** between reading `@0x8800` and writing `@0x8C00`!
- **Graphics II (`256×192` Bitmap) Byte & Pixel Formula**:
  With Pattern Table at `0x0000` (`VR4 = 0x03`), Color Table at `0x2000` (`VR3 = 0xFF`), Name Table at `0x1800` (`VR2 = 0x06`, initialized to `0..255` × 3), and `VR0 = 0x02`:
  `byte_offset = ((y & 0xFFF8) << 5) | (x & 0xFFF8) | (y & 7)` (`0..6143`), `bit_mask = 0x80 >> (x & 7)` (high byte `0x8000 >> (x & 7)`).

---

## 2. VDP Registers & Display Modes

- **`VR0`**: `0x00` (Graphics I / Text / Multicolor) or **`0x02`** (Graphics II / Bitmap mode `M3=1`).
- **`VR1`**:
  - Bit `0x80`: `1` = 16KB VRAM (`always 1` on TI-99/4A!)
  - Bit `0x40`: `1` = Display Blank OFF (`0` blanks screen for tear-free full-VRAM upload!)
  - Bit `0x20`: `1` = VBlank Interrupt Enable (`0xE0` = 16K + Display ON + IRQ ON)
  - Bit `0x10`: `M1=1` Text Mode (40×24, 6×8 cells, no sprites)
  - Bit `0x08`: `M2=1` Multicolor Mode (64×48 fat pixels)
  - Bit `0x02`: `SI=1` **16×16 Sprites** (`0` = 8×8 sprites)
  - Bit `0x01`: `MAG=1` **2× Magnified Sprites**
  - Common `VR1` values: **`0xE0`** (G1/G2, 8×8 sprites), **`0xE2`** (16×16 sprites), **`0xE1`** (8×8 mag 2×), **`0xE3`** (16×16 mag 2×), **`0xF0`** (40-col Text).
- **Table Base Formulas**:
  - `VR2` (Screen Image / Name Table): `VR2 * 0x0400` (e.g. `VR2=0x00` -> `0x0000`, 768 bytes = 32×24)
  - `VR3` (Color Table): `VR3 * 0x0040` in G1 (`VR3=0x0E` -> `0x0380`, 32 bytes = 1 byte per 8 chars); in G2 Bitmap: `VR3=0xFF` -> `0x2000` (6144 bytes)
  - `VR4` (Pattern Generator Table): `VR4 * 0x0800` in G1 (`VR4=0x01` -> `0x0800`, 2048 bytes); in G2 Bitmap: `VR4=0x03` -> `0x0000` (6144 bytes)
  - `VR5` (Sprite Attribute Table, SAT): `VR5 * 0x0080` (`VR5=0x06` -> **`0x0300`**, 128 bytes = 32 × 4-byte sprites)
  - `VR6` (Sprite Pattern Table): `VR6 * 0x0800` (`VR6=0x01` -> `0x0800`)
  - `VR7`: `(text_fg << 4) | backdrop_bg` (16-color palette: `0` Transparent, `1` Black, `2` Med Green, `4` Dark Blue, `6` Dark Red, `7` Cyan, `8` Med Red, `10` Dark Yellow, `11` Light Yellow, `14` Gray, `15` White).

---

## 3. TMS9918A Sprite Attribute Table (SAT) — Three Famous Hardware Quirks!

Each of the 32 sprites (`0..31`) in SAT (typically `0x0300`) is **4 bytes**: `[Y, X, Pattern, EarlyClock_Color]`:

1. **Quirk #1 — Scanline `Y - 1` Offset (`Y = 0xFF` is top row `0`!)**:
   The VDP evaluates sprites one scanline ahead, so hardware `Y_sat = (uint8_t)(screen_y - 1)`!
   - To render a sprite at top scanline **`screen_y = 0`**, write **`Y_sat = 0xFF` (`255` / `-1`)**!
   - To render at `screen_y = 40`, write **`Y_sat = 39` (`0x27`)**!
   - `Y_sat = 0xC0..0xCF` (`192..207`) hides a single sprite off the bottom of the 192px screen without terminating later sprites.
2. **Quirk #2 — Sprite Terminator Byte **`0xD0` (`208`)****:
   Writing **`0xD0`** as the `Y` byte of sprite `k` **immediately halts VDP rendering and collision detection for sprite `k` AND all higher-numbered sprites `k..31`**! (Note: `screen_y = 209` maps to `Y_sat = 208 = 0xD0`, so avoid `Y_sat == 0xD0` for active offscreen sprites!)
3. **Quirk #3 — Smooth Left-Edge Entry via the Early Clock Bit (`0x80`)**:
   `X_sat` is an unsigned byte `0..255`. To place a sprite at a **negative `screen_x` (`-32 <= screen_x < 0`)** so it enters smoothly from the left edge:
   - Set **`X_sat = (uint8_t)(screen_x + 32)`** (`0..31`), AND
   - Set **bit 7 (`0x80`, Early Clock)** in byte 3: **`Byte3 = 0x80 | (color & 0x0F)`** (shifts horizontal sprite clock 32 pixels left!).
   - When `screen_x >= 0`: `X_sat = (uint8_t)screen_x`, `Byte3 = color & 0x0F`.
4. **16×16 Sprites & 4-Per-Scanline Limit**:
   In 16×16 mode (`VR1` bit `0x02`), `Pattern` uses `pat & 0xFC` (4 consecutive 8×8 blocks: top-left, bottom-left, top-right, bottom-right = 32 bytes). At most **4 sprites** are drawn on any scanline (`VDPSTA` bit `0x40` + low 5 bits report the 5th dropped sprite).
