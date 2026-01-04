define i16 @test_gt(i16 %a, i16 %b) {
entry:
  %cmp = icmp sgt i16 %a, %b
  br i1 %cmp, label %is_greater, label %not_greater

is_greater:
  ret i16 42

not_greater:
  ret i16 0
}
