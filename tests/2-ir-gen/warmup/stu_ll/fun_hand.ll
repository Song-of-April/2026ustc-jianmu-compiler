define i32 @callee(i32 %a) {
entry:
  %result = mul i32 2, %a
  ret i32 %result
}

define i32 @main() {
entry:
  %result = call i32 @callee(i32 110)
  ret i32 %result
}
