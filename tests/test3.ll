define i16 @sub(i16 %a, i16 %b) {
  %result = sub i16 %a, %b
  ret i16 %result
}

define i16 @negate(i16 %a) {
  %result = sub i16 0, %a
  ret i16 %result
}
