; ============================================================================
; Exemplar 09: Graphics II Bitmap RMW, Console VBlank ISR (@0x83C4), & Hash Map
; Verified against full-system `ti99sim-0.16.0` (with real TI-99 bus wait states)
; ============================================================================

    .text
    .p2align 1
    .globl gfx2_plot_pixel_rmw
    .globl install_vblank_psg_fade
    .globl hm64_put
    .globl hm64_get

; ----------------------------------------------------------------------------
; 1. True VDP Read-Modify-Write Pixel Plotter in Graphics II Bitmap Mode
;    void gfx2_plot_pixel_rmw(uint16_t x, uint16_t y, uint16_t color_byte);
;    R0 = x (0..255), R1 = y (0..191), R2 = color_byte (low byte, e.g. 0x00F1)
;    Pattern Table @ 0x0000, Color Table @ 0x2000:
;      offset = ((y & 0xFFF8) << 5) | (x & 0xFFF8) | (y & 7)
;      mask   = 0x8000 >> (x & 7)  (high byte for SOCB!)
; ----------------------------------------------------------------------------
gfx2_plot_pixel_rmw:
    MOV  R0, R3                 ; R3 = x
    MOV  R1, R4                 ; R4 = y
    ANDI R4, 0xFFF8
    SLA  R4, 5                  ; (y & ~7) << 5
    MOV  R3, R5
    ANDI R5, 0xFFF8
    SOC  R5, R4                 ; | (x & ~7)
    ANDI R1, 0x0007
    SOC  R1, R4                 ; R4 = byte offset (0..6143)

    ; Build pixel bitmask 0x8000 >> (x & 7) in high byte of R5
    LI   R5, 0x8000
    ANDI R0, 0x0007             ; shift count in R0!
    JEQ  .Lmask_ok              ; CRITICAL: SRL Rx,0 when R0==0 shifts by 16!
    SRL  R5, 0
.Lmask_ok:
    ; Latch VDP READ address (R4, bit 6 of MSB = 0)
    SWPB R4
    MOVB R4, @0x8C02            ; LSB first
    SWPB R4
    MOVB R4, @0x8C02            ; MSB (read mode, prefetches VRAM[offset])
    MOVB @0x8800, R6            ; read existing pattern byte (advances VDP addr by +1!)
    SOCB R5, R6                 ; OR new pixel into high byte

    ; CRITICAL: Re-latch VDP WRITE address (0x4000 | offset) before writing @0x8C00!
    SWPB R4
    MOVB R4, @0x8C02
    SWPB R4
    ORI  R4, 0x4000
    MOVB R4, @0x8C02
    MOVB R6, @0x8C00            ; write updated Pattern[offset]

    ; Write Color[0x2000 + offset] (R4 already has 0x4000 | offset -> +0x2000 = 0x6000 | offset)
    AI   R4, 0x2000
    SWPB R4
    MOVB R4, @0x8C02
    SWPB R4
    MOVB R4, @0x8C02
    SWPB R2                     ; C uint8_t/uint16_t low byte -> high byte for MOVB!
    MOVB R2, @0x8C00
    B    *R11

; ----------------------------------------------------------------------------
; 2. Console ROM Level-1 VBlank User Hook (@0x83C4) + Safe LIMI 2/0 Frame Sync
;    Entered from Console ROM (0x0AB6) via `BL *R12` with WP = 0x83E0 (GPLWS)!
;    MUST preserve GPLWS R11, R13 (0x9800), R14 (0x01xx), R15 (0x8C02)
;    and return via `B *R11` (NEVER `RTWP`, NEVER `LWPI 0x8300`!).
; ----------------------------------------------------------------------------
    .bss
    .p2align 1
g_vblank_ticks:
    .space 2

    .text
    .p2align 1
install_vblank_psg_fade:
    ; R0 = target frame count
    LIMI 0
    CLR  @g_vblank_ticks
    LI   R2, 0x8000
    MOVB R2, @0x83C2            ; disable auto-sprite/sound/QUIT in Console ISR
    MOVB @0x8802, R2            ; clear any pending VDP interrupt flag
    LI   R2, .Lvblank_hook
    MOV  R2, @0x83C4            ; install user ISR hook
.Lwait_vb:
    LIMI 2                      ; open VBlank window safely (no foreground VDP access!)
    LIMI 0
    C    @g_vblank_ticks, R0
    JL   .Lwait_vb
    CLR  @0x83C4                ; unhook before returning
    MOV  @g_vblank_ticks, R0
    B    *R11

.Lvblank_hook:
    ; Executing in GPLWS (WP = 0x83E0): R0..R1 are safe scratch in GPLWS when not in GPL
    INC  @g_vblank_ticks
    MOV  @g_vblank_ticks, R0
    ANDI R0, 0x000F
    ORI  R0, 0x0090             ; Tone Ch 0 attenuation = 0x90 | (ticks & 15)
    SWPB R0
    MOVB R0, @0x8400
    B    *R11                   ; return to Console ROM 0x0AB8 (which runs LWPI 0x83C0; RTWP)

; ----------------------------------------------------------------------------
; 3. 64-Slot Open-Addressed 16-Bit Hash Map (`{key, val}`, empty = `0xFFFF`)
;    Uses SWPB+XOR bit mix and pre-scaled 4-byte slot mask `0x00FC`!
; ----------------------------------------------------------------------------
hm64_put:
    ; R0 = tab, R1 = key, R2 = val
    MOV  R1, R3
    SWPB R3
    XOR  R1, R3
    MOV  R3, R4
    SRL  R4, 3
    XOR  R4, R3
    ANDI R3, 0x00FC             ; 4-byte slot offset (0..252), zero SLA needed!
.Lput_lp:
    MOV  R3, R4
    A    R0, R4                 ; &tab[slot]
    MOV  *R4, R5
    C    R5, R1
    JEQ  .Lput_val              ; key already present -> update val in place
    CI   R5, 0xFFFF
    JEQ  .Lput_new
    AI   R3, 4                  ; MUST step by 4 bytes (NEVER INCT!)
    ANDI R3, 0x00FC
    JMP  .Lput_lp
.Lput_new:
    MOV  R1, *R4
.Lput_val:
    MOV  R2, @2(R4)
    B    *R11

hm64_get:
    ; R0 = tab, R1 = key -> R0 = val or -1 (0xFFFF)
    MOV  R1, R3
    SWPB R3
    XOR  R1, R3
    MOV  R3, R4
    SRL  R4, 3
    XOR  R4, R3
    ANDI R3, 0x00FC
    LI   R6, 64
.Lget_lp:
    MOV  R3, R4
    A    R0, R4
    MOV  *R4, R5
    C    R5, R1
    JEQ  .Lget_hit
    CI   R5, 0xFFFF
    JEQ  .Lget_miss
    AI   R3, 4
    ANDI R3, 0x00FC
    DEC  R6
    JNE  .Lget_lp
.Lget_miss:
    SETO R0
    B    *R11
.Lget_hit:
    MOV  @2(R4), R0
    B    *R11
