; Test multiple local variables - force memory use with volatile
@global = external global i16

define i16 @test_locals3(i16 %a, i16 %b) {
entry:
  %x = alloca i16
  %y = alloca i16
  store volatile i16 %a, ptr %x
  store volatile i16 %b, ptr %y
  %x_val = load volatile i16, ptr %x
  %y_val = load volatile i16, ptr %y
  %sum = add i16 %x_val, %y_val
  ret i16 %sum
}
