define i16 @test_eq(i16 %a, i16 %b) {
entry:
  %cmp = icmp eq i16 %a, %b
  br i1 %cmp, label %equal, label %not_equal

equal:
  ret i16 1

not_equal:
  ret i16 0
}
