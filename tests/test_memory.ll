; Test memory load/store operations
@global_var = global i16 0

; Test loading from a global variable
define i16 @load_global() {
entry:
  %val = load i16, ptr @global_var
  ret i16 %val
}

; Test storing to a global variable
define void @store_global(i16 %val) {
entry:
  store i16 %val, ptr @global_var
  ret void
}

; Test pointer dereference (indirect load)
define i16 @load_indirect(ptr %p) {
entry:
  %val = load i16, ptr %p
  ret i16 %val
}

; Test indirect store
define void @store_indirect(ptr %p, i16 %val) {
entry:
  store i16 %val, ptr %p
  ret void
}
