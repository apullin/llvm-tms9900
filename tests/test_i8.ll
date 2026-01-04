; Test 8-bit byte operations
define i8 @test_add8(i8 %a, i8 %b) {
entry:
  %result = add i8 %a, %b
  ret i8 %result
}

define i8 @test_sub8(i8 %a, i8 %b) {
entry:
  %result = sub i8 %a, %b
  ret i8 %result
}

define i8 @test_and8(i8 %a, i8 %b) {
entry:
  %result = and i8 %a, %b
  ret i8 %result
}

define i8 @test_or8(i8 %a, i8 %b) {
entry:
  %result = or i8 %a, %b
  ret i8 %result
}

define i8 @test_xor8(i8 %a, i8 %b) {
entry:
  %result = xor i8 %a, %b
  ret i8 %result
}

; Test load/store of bytes
define i8 @test_load8(ptr %p) {
entry:
  %val = load i8, ptr %p
  ret i8 %val
}

define void @test_store8(ptr %p, i8 %val) {
entry:
  store i8 %val, ptr %p
  ret void
}

; Test sign extension
define i16 @test_sext8(i8 %a) {
entry:
  %ext = sext i8 %a to i16
  ret i16 %ext
}

; Test zero extension
define i16 @test_zext8(i8 %a) {
entry:
  %ext = zext i8 %a to i16
  ret i16 %ext
}
