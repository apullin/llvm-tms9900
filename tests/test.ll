; Simple test for TMS9900 backend
define i16 @add(i16 %a, i16 %b) {
  %result = add i16 %a, %b
  ret i16 %result
}
