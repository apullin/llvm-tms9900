; Test multiple local variables
define i16 @test_locals2(i16 %a, i16 %b) {
entry:
  %x = alloca i16
  %y = alloca i16
  store i16 %a, ptr %x
  store i16 %b, ptr %y
  %x_val = load i16, ptr %x
  %y_val = load i16, ptr %y
  %sum = add i16 %x_val, %y_val
  ret i16 %sum
}
