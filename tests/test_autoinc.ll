; Test auto-increment addressing modes
; These patterns should use MOV *R+ when optimized

; Simple array sum - classic post-increment pattern
define i16 @sum_array(ptr %arr, i16 %count) {
entry:
  %cmp = icmp eq i16 %count, 0
  br i1 %cmp, label %exit, label %loop

loop:
  %ptr = phi ptr [ %arr, %entry ], [ %next_ptr, %loop ]
  %sum = phi i16 [ 0, %entry ], [ %new_sum, %loop ]
  %i = phi i16 [ %count, %entry ], [ %dec, %loop ]

  ; Load and increment pointer by 2 (word size)
  %val = load i16, ptr %ptr
  %next_ptr = getelementptr i16, ptr %ptr, i16 1

  %new_sum = add i16 %sum, %val
  %dec = add i16 %i, -1
  %done = icmp eq i16 %dec, 0
  br i1 %done, label %exit, label %loop

exit:
  %result = phi i16 [ 0, %entry ], [ %new_sum, %loop ]
  ret i16 %result
}

; Memory copy - another classic post-increment pattern
define void @memcpy_words(ptr %dst, ptr %src, i16 %count) {
entry:
  %cmp = icmp eq i16 %count, 0
  br i1 %cmp, label %exit, label %loop

loop:
  %src_ptr = phi ptr [ %src, %entry ], [ %next_src, %loop ]
  %dst_ptr = phi ptr [ %dst, %entry ], [ %next_dst, %loop ]
  %i = phi i16 [ %count, %entry ], [ %dec, %loop ]

  ; Load from source, store to dest, increment both pointers
  %val = load i16, ptr %src_ptr
  store i16 %val, ptr %dst_ptr

  %next_src = getelementptr i16, ptr %src_ptr, i16 1
  %next_dst = getelementptr i16, ptr %dst_ptr, i16 1

  %dec = add i16 %i, -1
  %done = icmp eq i16 %dec, 0
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

; Byte copy - should use MOVB *R+
define void @memcpy_bytes(ptr %dst, ptr %src, i16 %count) {
entry:
  %cmp = icmp eq i16 %count, 0
  br i1 %cmp, label %exit, label %loop

loop:
  %src_ptr = phi ptr [ %src, %entry ], [ %next_src, %loop ]
  %dst_ptr = phi ptr [ %dst, %entry ], [ %next_dst, %loop ]
  %i = phi i16 [ %count, %entry ], [ %dec, %loop ]

  ; Load byte from source, store to dest, increment both pointers
  %val = load i8, ptr %src_ptr
  store i8 %val, ptr %dst_ptr

  %next_src = getelementptr i8, ptr %src_ptr, i16 1
  %next_dst = getelementptr i8, ptr %dst_ptr, i16 1

  %dec = add i16 %i, -1
  %done = icmp eq i16 %dec, 0
  br i1 %done, label %exit, label %loop

exit:
  ret void
}
