; Test signed divide and remainder operations
define i16 @test_sdiv(i16 %a, i16 %b) {
entry:
  %result = sdiv i16 %a, %b
  ret i16 %result
}

define i16 @test_srem(i16 %a, i16 %b) {
entry:
  %result = srem i16 %a, %b
  ret i16 %result
}
