; Test switch statement code generation

; Small switch - probably should use compare chain
define i16 @small_switch(i16 %x) {
entry:
  switch i16 %x, label %default [
    i16 0, label %case0
    i16 1, label %case1
    i16 2, label %case2
  ]

case0:
  ret i16 100

case1:
  ret i16 200

case2:
  ret i16 300

default:
  ret i16 0
}

; Dense switch - good candidate for jump table
define i16 @dense_switch(i16 %x) {
entry:
  switch i16 %x, label %default [
    i16 0, label %case0
    i16 1, label %case1
    i16 2, label %case2
    i16 3, label %case3
    i16 4, label %case4
    i16 5, label %case5
    i16 6, label %case6
    i16 7, label %case7
  ]

case0:
  ret i16 10
case1:
  ret i16 20
case2:
  ret i16 30
case3:
  ret i16 40
case4:
  ret i16 50
case5:
  ret i16 60
case6:
  ret i16 70
case7:
  ret i16 80

default:
  ret i16 0
}

; Sparse switch - should use compare chain or binary search
define i16 @sparse_switch(i16 %x) {
entry:
  switch i16 %x, label %default [
    i16 1, label %case1
    i16 10, label %case10
    i16 100, label %case100
    i16 1000, label %case1000
  ]

case1:
  ret i16 1
case10:
  ret i16 2
case100:
  ret i16 3
case1000:
  ret i16 4

default:
  ret i16 0
}
