; Test divide and remainder operations
define i16 @test_udiv(i16 %a, i16 %b) {
entry:
  %result = udiv i16 %a, %b
  ret i16 %result
}

define i16 @test_urem(i16 %a, i16 %b) {
entry:
  %result = urem i16 %a, %b
  ret i16 %result
}
