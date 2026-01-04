; Test struct handling

; Simple struct with two fields
%struct.Point = type { i16, i16 }

; Return struct by value (small struct)
define %struct.Point @make_point(i16 %x, i16 %y) {
entry:
  %result = insertvalue %struct.Point undef, i16 %x, 0
  %result2 = insertvalue %struct.Point %result, i16 %y, 1
  ret %struct.Point %result2
}

; Extract fields from struct
define i16 @get_x(%struct.Point %p) {
entry:
  %x = extractvalue %struct.Point %p, 0
  ret i16 %x
}

define i16 @get_y(%struct.Point %p) {
entry:
  %y = extractvalue %struct.Point %p, 1
  ret i16 %y
}

; Load struct from pointer
define %struct.Point @load_point(ptr %p) {
entry:
  %val = load %struct.Point, ptr %p
  ret %struct.Point %val
}

; Store struct via pointer
define void @store_point(ptr %p, i16 %x, i16 %y) {
entry:
  %x_ptr = getelementptr inbounds %struct.Point, ptr %p, i32 0, i32 0
  store i16 %x, ptr %x_ptr
  %y_ptr = getelementptr inbounds %struct.Point, ptr %p, i32 0, i32 1
  store i16 %y, ptr %y_ptr
  ret void
}

; Pass struct pointer and modify
define void @move_point(ptr %p, i16 %dx, i16 %dy) {
entry:
  %x_ptr = getelementptr inbounds %struct.Point, ptr %p, i32 0, i32 0
  %y_ptr = getelementptr inbounds %struct.Point, ptr %p, i32 0, i32 1
  %x = load i16, ptr %x_ptr
  %y = load i16, ptr %y_ptr
  %new_x = add i16 %x, %dx
  %new_y = add i16 %y, %dy
  store i16 %new_x, ptr %x_ptr
  store i16 %new_y, ptr %y_ptr
  ret void
}

; Struct with mixed sizes
%struct.Mixed = type { i8, i16, i8 }

; Access byte fields in struct
define i8 @get_byte_field(ptr %p) {
entry:
  %field = getelementptr inbounds %struct.Mixed, ptr %p, i32 0, i32 0
  %val = load i8, ptr %field
  ret i8 %val
}

; Access word field with byte padding before it
define i16 @get_word_after_byte(ptr %p) {
entry:
  %field = getelementptr inbounds %struct.Mixed, ptr %p, i32 0, i32 1
  %val = load i16, ptr %field
  ret i16 %val
}

; Array of structs
define i16 @get_array_element_x(ptr %arr, i16 %idx) {
entry:
  %ptr = getelementptr inbounds %struct.Point, ptr %arr, i16 %idx, i32 0
  %val = load i16, ptr %ptr
  ret i16 %val
}
