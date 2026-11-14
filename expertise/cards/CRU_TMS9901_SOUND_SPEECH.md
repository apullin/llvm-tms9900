# TI-99/4A CRU, TMS9901 Input Matrix, & SN76489 Sound Card

---

## 1. CRU (Communication Register Unit) Laws & `LDCR`/`STCR` Byte Trap

- **`R12` Base Address Law**: `R12` holds **`2 × CRU_bit_number`** (because bit `A15` of the address bus is not wired to CRU—CRU bit address starts at `R12[14:3]`, i.e. `R12 >> 1`)!
  - Bit `0` (TMS9901 base): `LI R12, 0x0000`
  - Bit `18` (`0x12`, keyboard column select LSB): `LI R12, 0x0024` (`2 * 18 = 36 = 0x24`)!
  - Peripheral card DSR ROM enable: `LI R12, 0x1100` (disk), `SBO 0` enables `0x4000–0x5FFF` ROM; `SBZ 0` disables it.
- **Single-bit ops** (`disp` is signed `-128..+127` **bit** offset from `R12 >> 1`):
  - `SBO disp`: set CRU bit `(R12/2 + disp)` to `1` (12c)
  - `SBZ disp`: clear CRU bit `(R12/2 + disp)` to `0` (12c)
  - `TB disp`: test CRU bit `(R12/2 + disp)` -> sets **`EQ = 1` in `ST` when the CRU bit is `1`** (`JEQ` jumps when bit == `1`, `JNE` jumps when bit == `0`! Counter-intuitive: **`JEQ` means bit IS SET!**)
- **`LDCR src, count` / `STCR dst, count` HIGH-BYTE TRAP (`count = 0..15`, where `0` means `16` bits!)**:
  - When **`count` is `1..8`**: `LDCR` and `STCR` are **BYTE operations** — they transfer starting from the **LSB of the HIGH BYTE (`bit 8`, mask `0x0100`)** of a register!
    - Example: to output a 3-bit keyboard column `col` (`0..7` in low byte of `R1`) via `LDCR R1, 3`, you MUST first place it in the high byte (`SWPB R1` or `SLA R1, 8`)!
  - When **`count` is `9..15` or `0` (`16` bits)**: **WORD operation** — transfers starting from the **LSB of the full 16-bit word (`bit 15`, mask `0x0001`)**!

---

## 2. Raw TMS9901 Keyboard & Joystick Scanning (Zero Console ROM Overhead!)

Bypassing `KSCAN` (`BL @0x000E` takes ~5,000+ cycles!) via direct TMS9901 CRU takes **< 120 cycles** and allows **simultaneous multi-key rollover**:

- **Select column (`0..7`) + Alpha-Lock Diode Fix**:
  `LI R12, 0x0024` (CRU bits `18..20` = `P2..P4` column select; **CRU bit `21` = `P5` Alpha-Lock** — ALWAYS drive `P5` HIGH via `SBO 3` when `R12 = 0x0024`, or a pressed Alpha-Lock key shorts row `CRU bit 7` / Joystick `Up`!).
- **Single-`R12` Signed-Displacement Idiom**: Keep `R12 = 0x0024` (`CRU bit 18`) and test active-low rows (`CRU bits 3..10`: `0` = pressed -> `JNE`, `1` = released -> `JEQ`) directly with signed **`TB -15` .. `TB -8`** (`18 + disp`) without reloading `R12`!
  ```asm
  ; Select Joystick 1 (column 6 -> 0x0600 in high byte) with Alpha-Lock P5 HIGH:
      LI   R12, 0x0024        ; CRU bit 18 (column select)
      SBO  3                  ; CRU bit 21 (P5 Alpha-Lock = HIGH)
      LI   R0, 0x0600         ; col 6 in high byte
      LDCR R0, 3              ; output 3 bits to CRU 18..20
      LI   R12, 0x0006        ; CRU bit 3 (row inputs; or stay at 0x0024 and use TB -15..-11!)
      STCR R0, 5              ; read 5 bits into high byte of R0 (0 = pressed!)
      INV  R0                 ; now 1 = pressed: bit8=Fire, bit9=Left, bit10=Right, bit11=Down, bit12=Up
  ```
- **Exact Hardware Keyboard & Joystick Matrix (`Col 0..7` × `Row CRU Bit 3..10`, `0` = Pressed)**:

  | Row CRU Bit (`TB` from `0x0024`) | Col 0 | Col 1 | Col 2 | Col 3 | Col 4 | Col 5 | Col 6 (Joy 1) | Col 7 (Joy 2) |
  |---|---|---|---|---|---|---|---|---|
  | **Bit 3** (`TB -15`) | `=` | `.` | `,` | `M` | `N` | `/` | **Fire** | **Fire** |
  | **Bit 4** (`TB -14`) | **`Space`** | `L` | `K` | `J` | `H` | `;` | **Left** | **Left** |
  | **Bit 5** (`TB -13`) | `Enter` | `O` | `I` | `U` | `Y` | `P` | **Right** | **Right** |
  | **Bit 6** (`TB -12`) | — | `9` | `8` | `7` | `6` | `0` | **Down** | **Down** |
  | **Bit 7** (`TB -11`) | `Fctn` | `2` | `3` | `4` | `5` | `1` | **Up** | **Up** |
  | **Bit 8** (`TB -10`) | `Shift` | `S` | `D` | `F` | `G` | `A` | — | — |
  | **Bit 9** (`TB -9`) | `Ctrl` | `W` | `E` | `R` | `T` | `Q` | — | — |
  | **Bit 10** (`TB -8`) | — | `X` | `C` | `V` | `B` | `Z` | — | — |

- **TMS9901 14-Bit Hardware Interval Timer (`CRU 0x0000`, 1 tick = 64 CPU clocks = 21.3 µs)**:
  With `CLR R12`: `SBO 0` enters Clock Mode (and latches current countdown into the read register); bits `1..14` hold the 14-bit reload/read value; `SBZ 0` exits Clock Mode into I/O Mode and starts counting! **WARNING**: Transfer **`15` bits (`LDCR Rx, 15` / `STCR Rx, 15`), NEVER `0` (`16` bits)**—writing CRU bit `15` in Clock Mode triggers a TMS9901 Software Reset!

---

## 3. TMS9919 / SN76489 Sound Chip Protocol (`SOUND = 0x8400`)

All commands are written one byte at a time via **`MOVB Rx, @0x8400`** (data in the **HIGH byte** of `Rx`!).

### 3.1 Byte Framing (3 Tone Channels `0, 1, 2` + 1 Noise Channel `3`)

1. **Set 10-bit Tone Divider `N` (`0..1023`) on Channel `c` (`0..2`) — TWO BYTES**:
   - **Byte 1 (Latch + low 4 bits of `N`)**: `0x80 | (c << 5) | (N & 0x0F)`
     - Channel 0: `0x80 | (N & 0x0F)`
     - Channel 1: `0xA0 | (N & 0x0F)`
     - Channel 2: `0xC0 | (N & 0x0F)`
   - **Byte 2 (Data + upper 6 bits of `N`)**: `(N >> 4) & 0x3F` (bit 7 MUST be `0`!)
   - Frequency relation: $f_{\text{Hz}} = \frac{3,579,545}{32 \cdot N} \approx \frac{111,861}{N}$ (convert Hz in `R2` to `N` in 1 instruction: `LI R0, 0x0001; LI R1, 0xB4F5; DIV R2, R0` -> `R0 = N`!).
2. **Set 4-bit Attenuation `A` (`0` = max volume .. `15` (`0x0F`) = **SILENT / OFF**) — ONE BYTE**:
   - **Byte**: `0x90 | (c << 5) | (A & 0x0F)` (`c = 0`: `0x9x`, `c = 1`: `0xBx`, `c = 2`: `0xDx`, Noise `c = 3`: `0xFx`).
3. **Noise Control (`c = 3`) — ONE BYTE**:
   - `0xE0 | (type << 2) | (rate & 0x03)` (`type`: `0` periodic, `1` white noise; `rate`: `0..2` fixed, `3` tracks Tone Channel 2 frequency!).

### 3.2 Canonical 3-Byte Tone Setup Sequence in TMS9900 ASM
```asm
; R0 = chan (0..2), R1 = period10 (0..1023), R2 = atten (0..15)
    LI   R5, 0x8400         ; Sound port address
    SLA  R0, 5              ; R0 = chan << 5 (0x00, 0x20, or 0x40 in low byte)
    MOV  R1, R3
    ANDI R3, 0x000F
    SOC  R0, R3
    ORI  R3, 0x0080         ; Byte 1: 0x80 | (chan<<5) | (N & 0x0F)
    SWPB R3
    MOVB R3, *R5

    MOV  R1, R3
    SRL  R3, 4
    ANDI R3, 0x003F         ; Byte 2: (N >> 4) & 0x3F
    SWPB R3
    MOVB R3, *R5

    ANDI R2, 0x000F
    SOC  R0, R2
    ORI  R2, 0x0090         ; Byte 3: 0x90 | (chan<<5) | (atten & 0x0F)
    SWPB R2
    MOVB R2, *R5
    B    *R11
```
*(Always silence all 4 channels on startup if `LIMI 0`: write `0x9F, 0xBF, 0xDF, 0xFF` to `0x8400`!)*
