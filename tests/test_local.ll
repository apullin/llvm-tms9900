; Test local variables (stack allocation)
define i16 @test_local(i16 %a) {
entry:
  %local = alloca i16
  store i16 %a, ptr %local
  %loaded = load i16, ptr %local
  %result = add i16 %loaded, 1
  ret i16 %result
}
