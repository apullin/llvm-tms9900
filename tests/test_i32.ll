; Test 32-bit operations
define i32 @test_add32(i32 %a, i32 %b) {
entry:
  %result = add i32 %a, %b
  ret i32 %result
}

define i32 @test_sub32(i32 %a, i32 %b) {
entry:
  %result = sub i32 %a, %b
  ret i32 %result
}

define i32 @test_and32(i32 %a, i32 %b) {
entry:
  %result = and i32 %a, %b
  ret i32 %result
}

define i32 @test_or32(i32 %a, i32 %b) {
entry:
  %result = or i32 %a, %b
  ret i32 %result
}

define i32 @test_mul32(i32 %a, i32 %b) {
entry:
  %result = mul i32 %a, %b
  ret i32 %result
}

define i32 @test_sdiv32(i32 %a, i32 %b) {
entry:
  %result = sdiv i32 %a, %b
  ret i32 %result
}
