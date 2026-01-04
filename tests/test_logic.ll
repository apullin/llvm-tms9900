define i16 @and_imm(i16 %a) {
  %result = and i16 %a, 255
  ret i16 %result
}

define i16 @or_imm(i16 %a) {
  %result = or i16 %a, 255
  ret i16 %result
}

define i16 @xor_reg(i16 %a, i16 %b) {
  %result = xor i16 %a, %b
  ret i16 %result
}

define i16 @or_reg(i16 %a, i16 %b) {
  %result = or i16 %a, %b
  ret i16 %result
}

define i16 @not_op(i16 %a) {
  %result = xor i16 %a, -1
  ret i16 %result
}
