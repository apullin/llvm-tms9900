; life3_2x_step.S - Ultra-fast Separable PAD-Resident CSA Engine & VDP Streamer
;
; Exports:
;   void life_compute_row(uint16_t *row_prev, uint16_t *row,
;                         uint16_t *row_next, uint16_t *row_dst)
;   void step_board(uint8_t (*src)[16], uint8_t (*dst)[16])
;   void vdp_write_dirty_patterns(uint8_t (*b)[16])
;
; Key domain optimizations applied:
;   1. Exact 2-instruction `HA(L,R)` recovery from buffered `FA(L,C,R)`:
;        ms = hs_m ^ mC
;        mc = hc_m & ~ms   (`SZC ms, hc_m`!)
;      Combines 3-row separable horizontal reuse (`3x` fewer shifts/wraps/FAs)
;      with the 5-instruction 8-neighbor Conway rule and 1-op dirty `mC ^ res`!
;   2. Dense 8-word vertical kernel (`pad_dense_vrow`, 74 bytes) executes
;      directly out of 16-bit 0-wait Scratchpad PAD (`0x8320..0x8369`), while
;      leaving `0x8300..0x831F` (WP) and `0x8374..0x837F` (KSCAN/VDP) untouched!
;   3. Unrolled horizontal row builder (`HWORD` macro) with 1st-word empty-row
;      fast exit and per-word zero-neighborhood skip.
;   4. Word-loaded assembly VDP row streamer using `SWPB` and register-resident
;      `e0..e3` (zero inner-loop spills).

        .bss
        .p2align 1
old_active_buf:
        .space  24
dirty_acc_buf:
        .space  16
slot2_buf:
        .space  32
win_ptrs_and_nz:
        .space  34              ; BSS state scalars (never touches KSCAN PAD!)

        .equ    W_BUF_U,       win_ptrs_and_nz + 0
        .equ    W_BUF_M,       win_ptrs_and_nz + 2
        .equ    W_BUF_D,       win_ptrs_and_nz + 4
        .equ    W_NZ_U,        win_ptrs_and_nz + 6
        .equ    W_NZ_M,        win_ptrs_and_nz + 8
        .equ    W_NZ_D,        win_ptrs_and_nz + 10
        .equ    W_SAVED_LR,    win_ptrs_and_nz + 12
        .equ    W_SAVED_SP,    win_ptrs_and_nz + 14
        .equ    PAD_SRC_BASE,  win_ptrs_and_nz + 16
        .equ    PAD_DST_BASE,  win_ptrs_and_nz + 18
        .equ    PAD_CUR_TY,    win_ptrs_and_nz + 20
        .equ    PAD_SRC_PTR,   win_ptrs_and_nz + 22
        .equ    PAD_DST_PTR,   win_ptrs_and_nz + 24
        .equ    PAD_ROW_LIVE,  win_ptrs_and_nz + 26
        .equ    PAD_DY_CNT,    win_ptrs_and_nz + 28
        .equ    PAD_HAVE_WIN,  win_ptrs_and_nz + 30
        .equ    PAD_VDP_TMP,   win_ptrs_and_nz + 32

; KSCAN-safe Scratchpad PAD map:
;   0x8300..0x831F = Workspace (WP = 0x8300)
;   0x8320..0x8373 = PAD_CODE (42 words = 84B; stops strictly before 0x8374!)
;   0x8374..0x837F = UNTOUCHED (KSCAN key/mode/status)
;   0x8380..0x839F = PAD_SLOT0 (32B)
;   0x83A0..0x83BF = PAD_SLOT1 (32B; stops strictly before 0x83C0!)
;   0x83C0..0x83FF = UNTOUCHED (KSCAN state 0x83C6..0x83CA, VDPR1 shadow 0x83D4, GPLWS 0x83E0..0x83FF!)
        .equ    PAD_CODE,      0x8320
        .equ    PAD_SLOT0,     0x8380
        .equ    PAD_SLOT1,     0x83A0
        .equ    PAD_SLOT2,     slot2_buf
        .equ    PAD_DIRTY_ACC, dirty_acc_buf

        .text
        .p2align 1
        .globl  life_compute_row
        .globl  step_board
        .globl  life_step_begin
        .globl  life_step_end
        .globl  vdp_write_dirty_patterns
        .globl  life_draw_begin
        .globl  life_draw_end

; Macro for one horizontal 16-cell word `w`:
;   w_left = \rl (clobbered), w_cur = \rc (preserved!), w_right = \rr (preserved!)
;   Writes `(hs, hc)` to `*R2+`, sets `\mask_bit` in `R0` if non-zero.
        .macro  HWORD rl, rc, rr, mask_bit
        MOV  \rc, \rc
        JNE  1f
        MOV  \rl, R4
        SRL  R4, 1
        JOC  1f
        MOV  \rr, \rr
        JLT  1f
        CLR  *R2+
        CLR  *R2+
        JMP  4f
1:
        ORI  R0, \mask_bit
        MOV  \rc, R4
        SRL  R4, 1
        SRL  \rl, 1
        JNC  2f
        ORI  R4, 0x8000
2:
        MOV  \rc, R8
        SLA  R8, 1
        MOV  \rr, R9
        SLA  R9, 1
        JNC  3f
        INC  R8
3:
        MOV  R4, R13
        XOR  \rc, R4            ; x = L ^ C
        INV  R13
        MOV  \rc, R14
        SZC  R13, R14           ; C & L
        MOV  R4, R13
        XOR  R8, R4             ; hs = x ^ R
        MOV  R4, *R2+
        INV  R13
        SZC  R13, R8            ; R & x
        SOC  R14, R8            ; hc = (C & L) | (R & x)
        MOV  R8, *R2+
4:
        .endm

; ============================================================================
; compute_hrow:
;   R1 = &row[0] (8 words), R2 = &pad_slot[0] (16 words in 0-wait PAD)
;   Returns R0 = 8-bit non-zero word mask in [15:8]
; ============================================================================
compute_hrow:
        ; Fast 1st-word check: dense rows exit in 2 instructions instead of 9!
        MOV  *R1+, R6           ; R6 = w0
        JNE  .Lh_w0_nz
        MOV  *R1+, R7           ; w1
        JNE  .Lh_scan_nz2
        SOC  *R1+, R7           ; w2
        SOC  *R1+, R7           ; w3
        SOC  *R1+, R7           ; w4
        SOC  *R1+, R7           ; w5
        SOC  *R1+, R7           ; w6
        SOC  *R1+, R7           ; w7
        JNE  .Lh_rewind_all

        ; Entire 128-cell row is zero: clear 16 PAD words & return R0 = 0
        CLR  R0
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        B    *R11

.Lh_rewind_all:
        AI   R1, -14            ; R1 -> &row[1] (R6 is already 0 = row[0]!)
        JMP  .Lh_w0_nz
.Lh_scan_nz2:
        DECT R1                 ; R1 -> &row[1] (R6 is already 0 = row[0]!)

.Lh_w0_nz:
        ; R1 currently points at `&row[1]`, and R6 already holds `row[0]`!
        MOV  R6, R15            ; R15 = row[0] (saved for word 7 right wrap)
        MOV  @12(R1), R5        ; R5  = row[7] (`&row[1] + 12 == &row[7]`)
        CLR  R0                 ; R0  = nz_mask

        ; Unrolled rotating registers `(R5, R6, R7)` — zero register-move copies!
        MOV  *R1+, R7           ; R7 = row[1]
        HWORD R5, R6, R7, 0x8000

        MOV  *R1+, R5           ; R5 = row[2]
        HWORD R6, R7, R5, 0x4000

        MOV  *R1+, R6           ; R6 = row[3]
        HWORD R7, R5, R6, 0x2000

        MOV  *R1+, R7           ; R7 = row[4]
        HWORD R5, R6, R7, 0x1000

        MOV  *R1+, R5           ; R5 = row[5]
        HWORD R6, R7, R5, 0x0800

        MOV  *R1+, R6           ; R6 = row[6]
        HWORD R7, R5, R6, 0x0400

        MOV  *R1+, R7           ; R7 = row[7]
        HWORD R5, R6, R7, 0x0200

        HWORD R6, R7, R15, 0x0100
        B    *R11

; ============================================================================
; Relocatable 37-word (74-byte) dense vertical CSA loop copied to 0x8320 PAD!
; Inputs:
;   R0  = 8 (word count)
;   R1  = &src[y][0], R2 = &dst[y][0]
;   R3  = buf_u, R4 = buf_m, R5 = buf_d
;   R11 = &PAD_DIRTY_ACC[0], R13 = saved return PC
; ============================================================================
pad_vrow_template:
.Lpv_loop:
        JOC  .Lpv_active
        B    @.Lpv_skip_rom
.Lpv_active:
        MOV  *R1+, R14          ; mC
        MOV  *R3+, R6           ; hs_u
        MOV  *R4+, R7           ; hs_m
        XOR  R14, R7            ; ms = hs_m ^ mC
        MOV  *R4+, R15          ; hc_m
        SZC  R7, R15            ; mc = hc_m & ~ms
        MOV  *R5+, R8           ; hs_d

        MOV  R6, R12
        XOR  R7, R6
        INV  R12
        SZC  R12, R7
        MOV  R6, R12
        XOR  R8, R6             ; R6 = bit0
        INV  R12
        SZC  R12, R8
        SOC  R7, R8             ; R8 = c1

        MOV  *R3+, R7           ; hc_u
        MOV  *R5+, R9           ; hc_d
        MOV  R7, R12
        XOR  R15, R7
        INV  R12
        SZC  R12, R15
        MOV  R7, R12
        XOR  R9, R7             ; R7 = s2
        INV  R12
        SZC  R12, R9
        SOC  R15, R9            ; R9 = c2

        XOR  R8, R7
        SZC  R9, R7
        SOC  R14, R6
        INV  R6
        SZC  R6, R7             ; R7 = res

        MOV  R7, *R2+
        SOC  R7, R10            ; R10 = row_live_acc (in 0-wait PAD workspace!)
        XOR  R7, R14
        SOC  R14, *R11+

.Lpv_next:
        SLA  R0, 1
        JNE  .Lpv_loop
        B    *R13
pad_vrow_template_end:

.Lpv_skip_rom:
        INCT R1
        CLR  *R2+
        AI   R3, 4
        AI   R4, 4
        AI   R5, 4
        INCT R11
        B    @(PAD_CODE + (.Lpv_next - pad_vrow_template))

combine_vrow_pad:
        MOV  R0, R0
        JEQ  .Lv_all_zero
        MOV  R11, @W_SAVED_LR
        MOV  R10, @W_SAVED_SP
        MOV  @PAD_ROW_LIVE, R10
        LI   R13, .Lpv_done
        MOV  @W_BUF_U, R3
        MOV  @W_BUF_M, R4
        MOV  @W_BUF_D, R5
        LI   R11, PAD_DIRTY_ACC
        ANDI R0, 0xFF00
        ORI  R0, 0x0080
        SLA  R0, 1
        B    @PAD_CODE
.Lpv_done:
        MOV  R10, @PAD_ROW_LIVE
        MOV  @W_SAVED_SP, R10
        MOV  @W_SAVED_LR, R11
        B    *R11

.Lv_all_zero:
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        AI   R1, 16
        B    *R11

; ============================================================================
; Standalone C-callable row entry (for `row_tests` harness):
; ============================================================================
life_compute_row:
        DECT R10
        MOV  R11, *R10
        DECT R10
        MOV  R9, *R10
        DECT R10
        MOV  R13, *R10
        DECT R10
        MOV  R14, *R10
        DECT R10
        MOV  R15, *R10
        DECT R10
        MOV  R1, *R10
        DECT R10
        MOV  R2, *R10
        DECT R10
        MOV  R3, *R10

        LI   R4, pad_vrow_template
        LI   R5, PAD_CODE
        LI   R6, (pad_vrow_template_end - pad_vrow_template) / 2
.Llcr_cp:
        MOV  *R4+, *R5+
        DEC  R6
        JNE  .Llcr_cp

        MOV  R0, R1
        LI   R2, PAD_SLOT0
        BL   @compute_hrow
        MOV  R0, @W_NZ_U

        MOV  @4(R10), R1
        LI   R2, PAD_SLOT1
        BL   @compute_hrow
        MOV  R0, @W_NZ_M

        MOV  @2(R10), R1
        LI   R2, PAD_SLOT2
        BL   @compute_hrow

        SOC  @W_NZ_U, R0
        SOC  @W_NZ_M, R0
        LI   R3, PAD_SLOT0
        MOV  R3, @W_BUF_U
        LI   R3, PAD_SLOT1
        MOV  R3, @W_BUF_M
        LI   R3, PAD_SLOT2
        MOV  R3, @W_BUF_D

        MOV  *R10+, R2
        AI   R10, 2
        MOV  *R10+, R1
        BL   @combine_vrow_pad

        MOV  *R10+, R15
        MOV  *R10+, R14
        MOV  *R10+, R13
        MOV  *R10+, R9
        MOV  *R10+, R11
        B    *R11

; ============================================================================
; Full-board generation step:
;   void step_board(uint8_t (*src)[16] [R0], uint8_t (*dst)[16] [R1])
; ============================================================================
        .section .text.step_board,"ax",@progbits
        .p2align 1
step_board:
life_step_begin:
        DECT R10
        MOV  R11, *R10
        DECT R10
        MOV  R13, *R10
        DECT R10
        MOV  R14, *R10
        DECT R10
        MOV  R15, *R10

        MOV  R0, @PAD_SRC_BASE
        MOV  R1, @PAD_DST_BASE
        MOV  R0, @PAD_SRC_PTR
        MOV  R1, @PAD_DST_PTR
        CLR  @PAD_HAVE_WIN

        ; Install `pad_vrow_template` into 0-wait PAD (`0x8380..0x83E9`) on every step!
        LI   R2, pad_vrow_template
        LI   R3, PAD_CODE
        LI   R4, (pad_vrow_template_end - pad_vrow_template) / 2
.Lsb_cp_pad:
        MOV  *R2+, *R3+
        DEC  R4
        JNE  .Lsb_cp_pad

        ; Copy `active_rows[0..23]` (12 words) to `old_active_buf` & clear `active_rows`
        LI   R2, active_rows
        LI   R3, old_active_buf
        LI   R4, 12
.Lsb_cp_act:
        MOV  *R2, *R3+
        CLR  *R2+
        DEC  R4
        JNE  .Lsb_cp_act

        CLR  @PAD_CUR_TY

.Lsb_ty_loop:
        MOV  @PAD_CUR_TY, R4
        MOVB @old_active_buf(R4), R0
        JNE  .Lsb_ty_active

        ; ---- Inactive tile row ----
        CLR  @PAD_HAVE_WIN
        MOV  @PAD_DST_PTR, R2
        LI   R3, 4
.Lsb_clr_dst:
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        DEC  R3
        JNE  .Lsb_clr_dst
        MOV  R2, @PAD_DST_PTR

        MOV  @PAD_SRC_PTR, R2
        AI   R2, 64
        MOV  R2, @PAD_SRC_PTR

        MOV  @PAD_CUR_TY, R2
        SLA  R2, 5
        AI   R2, dirty_tiles
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        B    @.Lsb_next_ty

.Lsb_ty_active:
        LI   R2, PAD_DIRTY_ACC
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  *R2+
        CLR  @PAD_ROW_LIVE

        MOV  @PAD_HAVE_WIN, R0
        JNE  .Lsb_win_ready

        LI   R3, PAD_SLOT0
        MOV  R3, @W_BUF_U
        LI   R3, PAD_SLOT1
        MOV  R3, @W_BUF_M
        LI   R3, PAD_SLOT2
        MOV  R3, @W_BUF_D

        MOV  @PAD_CUR_TY, R0
        JNE  .Lsb_prev_norm
        MOV  @PAD_SRC_BASE, R1
        AI   R1, 1520
        JMP  .Lsb_do_prev
.Lsb_prev_norm:
        MOV  @PAD_SRC_PTR, R1
        AI   R1, -16
.Lsb_do_prev:
        MOV  @W_BUF_U, R2
        BL   @compute_hrow
        MOV  R0, @W_NZ_U

        MOV  @PAD_SRC_PTR, R1
        MOV  @W_BUF_M, R2
        BL   @compute_hrow
        MOV  R0, @W_NZ_M

        SETO @PAD_HAVE_WIN

.Lsb_win_ready:
        LI   R0, 4
        MOV  R0, @PAD_DY_CNT

.Lsb_dy_loop:
        MOV  @PAD_SRC_PTR, R1
        AI   R1, 16
        MOV  @PAD_CUR_TY, R0
        CI   R0, 23
        JNE  .Lsb_do_next
        MOV  @PAD_DY_CNT, R0
        CI   R0, 1
        JNE  .Lsb_do_next
        MOV  @PAD_SRC_BASE, R1
.Lsb_do_next:
        MOV  @W_BUF_D, R2
        BL   @compute_hrow
        MOV  R0, @W_NZ_D

        SOC  @W_NZ_U, R0
        SOC  @W_NZ_M, R0
        MOV  @PAD_SRC_PTR, R1
        MOV  @PAD_DST_PTR, R2
        BL   @combine_vrow_pad
        MOV  R1, @PAD_SRC_PTR
        MOV  R2, @PAD_DST_PTR

        MOV  @W_BUF_U, R0
        MOV  @W_BUF_M, @W_BUF_U
        MOV  @W_BUF_D, @W_BUF_M
        MOV  R0, @W_BUF_D
        MOV  @W_NZ_M, @W_NZ_U
        MOV  @W_NZ_D, @W_NZ_M

        DEC  @PAD_DY_CNT
        JNE  .Lsb_dy_loop

        ; Write `dirty_tiles[ty][0..31]` from `PAD_DIRTY_ACC[0..7]`
        MOV  @PAD_CUR_TY, R2
        SLA  R2, 5
        AI   R2, dirty_tiles
        LI   R1, PAD_DIRTY_ACC
        LI   R3, 8
        LI   R6, 0x0100
.Lsb_dirty_w:
        MOV  *R1+, R0
        JNE  .Lsb_dirty_nz
        CLR  *R2+
        CLR  *R2+
        DEC  R3
        JNE  .Lsb_dirty_w
        JMP  .Lsb_dirty_done
.Lsb_dirty_nz:
        CLR  R4
        MOV  R0, R5
        ANDI R5, 0xF000
        JEQ  .Lsb_dn0
        MOVB R6, R4
.Lsb_dn0:
        MOV  R0, R5
        ANDI R5, 0x0F00
        JEQ  .Lsb_dn1
        INC  R4
.Lsb_dn1:
        MOV  R4, *R2+
        CLR  R4
        MOV  R0, R5
        ANDI R5, 0x00F0
        JEQ  .Lsb_dn2
        MOVB R6, R4
.Lsb_dn2:
        ANDI R0, 0x000F
        JEQ  .Lsb_dn3
        INC  R4
.Lsb_dn3:
        MOV  R4, *R2+
        DEC  R3
        JNE  .Lsb_dirty_w

.Lsb_dirty_done:
        MOV  @PAD_ROW_LIVE, R0
        JEQ  .Lsb_next_ty

        LI   R6, 0x0100
        MOV  @PAD_CUR_TY, R4
        MOVB R6, @active_rows(R4)

        MOV  R4, R5
        JNE  .Lsb_act_prev_ok
        LI   R5, 24
.Lsb_act_prev_ok:
        DEC  R5
        MOVB R6, @active_rows(R5)

        INC  R4
        CI   R4, 24
        JNE  .Lsb_act_next_ok
        CLR  R4
.Lsb_act_next_ok:
        MOVB R6, @active_rows(R4)

.Lsb_next_ty:
        INC  @PAD_CUR_TY
        MOV  @PAD_CUR_TY, R0
        CI   R0, 24
        JEQ  .Lsb_done
        B    @.Lsb_ty_loop

.Lsb_done:
        MOV  *R10+, R15
        MOV  *R10+, R14
        MOV  *R10+, R13
        MOV  *R10+, R11
life_step_end:
        B    *R11

; ============================================================================
; Assembly VDP Dirty-Row Pattern Streamer:
;   void vdp_write_dirty_patterns(uint8_t (*b)[16] [R0])
; ============================================================================
        .section .text.vdp_write_dirty_patterns,"ax",@progbits
        .p2align 1
vdp_write_dirty_patterns:
life_draw_begin:
        DECT R10
        MOV  R11, *R10
        DECT R10
        MOV  R13, *R10
        DECT R10
        MOV  R14, *R10
        DECT R10
        MOV  R15, *R10

        MOV  R0, R1             ; R1 = &b[0][0]
        LI   R2, dirty_tiles    ; R2 = &dirty_tiles[0][0]
        LI   R11, 0x4000        ; high byte of VDP write cmd (`0x40 + ty` in [15:8])
        LI   R12, 24            ; 24 tile rows

.Lvd_row_loop:
        MOV  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        SOC  *R2+, R0
        JNE  .Lvd_row_dirty

        AI   R1, 64
        AI   R11, 0x0100
        DEC  R12
        JNE  .Lvd_row_loop
        JMP  .Lvd_done

.Lvd_row_dirty:
        CLR  R0
        MOVB R0, @0x8C02
        MOVB R11, @0x8C02

        ; Save R2 (`dirty_tiles` ptr) once per row in PAD so R2 is free for `e2`!
        MOV  R2, @PAD_VDP_TMP

        MOV  R1, R3
        AI   R3, 16
        MOV  R3, R4
        AI   R4, 16
        MOV  R4, R5
        AI   R5, 16

        LI   R0, 8              ; 8 word-columns

.Lvd_word_col:
        MOV  *R1+, R6           ; {b0_even, b0_odd}
        MOV  *R3+, R7           ; {b1_even, b1_odd}
        MOV  *R4+, R8           ; {b2_even, b2_odd}
        MOV  *R5+, R9           ; {b3_even, b3_odd}

        ; ---- Even byte-column (use `@0x831F` R15 low-byte PAD alias — saves 10 clks/byte!) ----
        CLR  R15
        MOVB R6, @0x831F
        A    R15, R15
        MOV  @expand8(R15), R13 ; e0
        MOVB R13, @0x8C00
        MOVB R13, @0x8C00

        CLR  R15
        MOVB R7, @0x831F
        A    R15, R15
        MOV  @expand8(R15), R14 ; e1
        MOVB R14, @0x8C00
        MOVB R14, @0x8C00

        CLR  R15
        MOVB R8, @0x831F
        A    R15, R15
        MOV  @expand8(R15), R2  ; e2
        MOVB R2, @0x8C00
        MOVB R2, @0x8C00

        CLR  R15
        MOVB R9, @0x831F
        A    R15, R15
        MOV  @expand8(R15), R15 ; e3
        MOVB R15, @0x8C00
        MOVB R15, @0x8C00

        SWPB R13
        MOVB R13, @0x8C00
        MOVB R13, @0x8C00
        SWPB R14
        MOVB R14, @0x8C00
        MOVB R14, @0x8C00
        SWPB R2
        MOVB R2, @0x8C00
        MOVB R2, @0x8C00
        SWPB R15
        MOVB R15, @0x8C00
        MOVB R15, @0x8C00

        ; ---- Odd byte-column (low bytes of R6..R9) ----
        ANDI R6, 0x00FF
        A    R6, R6
        MOV  @expand8(R6), R6
        MOVB R6, @0x8C00
        MOVB R6, @0x8C00

        ANDI R7, 0x00FF
        A    R7, R7
        MOV  @expand8(R7), R7
        MOVB R7, @0x8C00
        MOVB R7, @0x8C00

        ANDI R8, 0x00FF
        A    R8, R8
        MOV  @expand8(R8), R8
        MOVB R8, @0x8C00
        MOVB R8, @0x8C00

        ANDI R9, 0x00FF
        A    R9, R9
        MOV  @expand8(R9), R9
        MOVB R9, @0x8C00
        MOVB R9, @0x8C00

        SWPB R6
        MOVB R6, @0x8C00
        MOVB R6, @0x8C00
        SWPB R7
        MOVB R7, @0x8C00
        MOVB R7, @0x8C00
        SWPB R8
        MOVB R8, @0x8C00
        MOVB R8, @0x8C00
        SWPB R9
        MOVB R9, @0x8C00
        MOVB R9, @0x8C00

        DEC  R0
        JNE  .Lvd_word_col

        MOV  @PAD_VDP_TMP, R2
        AI   R1, 48
        AI   R11, 0x0100
        DEC  R12
        JEQ  .Lvd_done
        B    @.Lvd_row_loop

.Lvd_done:
        MOV  *R10+, R15
        MOV  *R10+, R14
        MOV  *R10+, R13
        MOV  *R10+, R11
life_draw_end:
        B    *R11
