define i16 @max(i16 %a, i16 %b) {
entry:
  %cmp = icmp sgt i16 %a, %b
  br i1 %cmp, label %if.then, label %if.else

if.then:
  ret i16 %a

if.else:
  ret i16 %b
}

define i16 @abs_val(i16 %a) {
entry:
  %cmp = icmp slt i16 %a, 0
  br i1 %cmp, label %if.neg, label %if.pos

if.neg:
  %neg = sub i16 0, %a
  ret i16 %neg

if.pos:
  ret i16 %a
}
