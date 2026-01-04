; Test 32-bit operations with inline expansion

; 32-bit left shift by constant
define i32 @shift_left_4(i32 %x) {
entry:
  %result = shl i32 %x, 4
  ret i32 %result
}

; 32-bit right shift by constant (logical)
define i32 @shift_right_4(i32 %x) {
entry:
  %result = lshr i32 %x, 4
  ret i32 %result
}

; 32-bit right shift by constant (arithmetic)
define i32 @shift_right_arith_4(i32 %x) {
entry:
  %result = ashr i32 %x, 4
  ret i32 %result
}

; 32-bit shift by 16 (word swap)
define i32 @shift_left_16(i32 %x) {
entry:
  %result = shl i32 %x, 16
  ret i32 %result
}

; 32-bit multiply (should expand to partial products)
define i32 @mul32(i32 %a, i32 %b) {
entry:
  %result = mul i32 %a, %b
  ret i32 %result
}

; 32-bit add (should expand to add with carry synthesis)
define i32 @add32(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}

; 32-bit subtract
define i32 @sub32(i32 %a, i32 %b) {
entry:
  %result = sub i32 %a, %b
  ret i32 %result
}

; Division still uses libcall
define i32 @div32(i32 %a, i32 %b) {
entry:
  %result = sdiv i32 %a, %b
  ret i32 %result
}
