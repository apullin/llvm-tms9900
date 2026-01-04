; Comprehensive test of TMS9900 backend features
@global = global i16 0

; Test basic arithmetic and function call
define i16 @add_values(i16 %a, i16 %b) {
entry:
  %sum = add i16 %a, %b
  ret i16 %sum
}

; Test comparison and branching
define i16 @max_value(i16 %a, i16 %b) {
entry:
  %cmp = icmp sgt i16 %a, %b
  br i1 %cmp, label %then, label %else

then:
  ret i16 %a

else:
  ret i16 %b
}

; Test local variables with function call
define i16 @test_with_call(i16 %a, i16 %b) {
entry:
  %local = alloca i16
  %sum = call i16 @add_values(i16 %a, i16 %b)
  store i16 %sum, ptr %local
  %loaded = load i16, ptr %local
  ret i16 %loaded
}

; Test shifts
define i16 @shift_test(i16 %val) {
entry:
  %shl = shl i16 %val, 2
  %shr = ashr i16 %shl, 1
  ret i16 %shr
}

; Test logical operations
define i16 @logic_test(i16 %a, i16 %b) {
entry:
  %or = or i16 %a, %b
  %xor = xor i16 %or, %a
  ret i16 %xor
}
