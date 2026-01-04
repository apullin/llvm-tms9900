; Test inline assembly support

; Basic inline asm with no operands
define void @test_nop() {
entry:
  call void asm sideeffect "NOP", ""()
  ret void
}

; Inline asm with register input
define i16 @test_inc(i16 %val) {
entry:
  %result = call i16 asm "INC $0", "=r,0"(i16 %val)
  ret i16 %result
}

; Inline asm with two register inputs
define i16 @test_add_asm(i16 %a, i16 %b) {
entry:
  %result = call i16 asm "A $2,$0", "=r,0,r"(i16 %a, i16 %b)
  ret i16 %result
}

; Inline asm with specific register constraint
define i16 @test_specific_reg(i16 %val) {
entry:
  %result = call i16 asm "SWPB $0", "={R0},{R0}"(i16 %val)
  ret i16 %result
}

; Inline asm with immediate
define void @test_limi() {
entry:
  call void asm sideeffect "LIMI $0", "i"(i16 0)
  ret void
}

; CRU instruction example (set bit output) - uses register to hold CRU address
define void @test_sbo(i16 %bit) {
entry:
  call void asm sideeffect "SBO $0", "r"(i16 %bit)
  ret void
}

; Using a register as a pointer (indirect addressing in the asm itself)
define void @test_clr_indirect(ptr %addr) {
entry:
  call void asm sideeffect "CLR *$0", "r"(ptr %addr)
  ret void
}

; Test clobbers - instruction that modifies R12
define void @test_clobber(i16 %val) {
entry:
  call void asm sideeffect "LDCR $0,8", "r,~{R12}"(i16 %val)
  ret void
}
