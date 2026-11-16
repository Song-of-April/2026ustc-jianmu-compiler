define i32 @main() {
entry:
  %a = alloca [10 x i32]
  %first = getelementptr [10 x i32], [10 x i32]* %a, i32 0, i32 0
  store i32 10, i32* %first
  %x = load i32, i32* %first
  %twice = mul i32 %x, 2
  %second = getelementptr [10 x i32], [10 x i32]* %a, i32 0, i32 1
  store i32 %twice, i32* %second
  %result = load i32, i32* %second
  ret i32 %result
}
