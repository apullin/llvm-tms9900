declare i16 @external(i16)

define i16 @caller(i16 %x) {
  %result = call i16 @external(i16 %x)
  ret i16 %result
}
