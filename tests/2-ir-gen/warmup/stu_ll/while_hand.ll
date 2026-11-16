define i32 @main() {
entry:
  %a = alloca i32
  %i = alloca i32
  store i32 10, i32* %a
  store i32 0, i32* %i
  br label %test

test:
  %old_i = load i32, i32* %i
  %condition = icmp slt i32 %old_i, 10
  br i1 %condition, label %body, label %done

body:
  %next_i = add i32 %old_i, 1
  store i32 %next_i, i32* %i
  %old_a = load i32, i32* %a
  %next_a = add i32 %old_a, %next_i
  store i32 %next_a, i32* %a
  br label %test

done:
  %result = load i32, i32* %a
  ret i32 %result
}
