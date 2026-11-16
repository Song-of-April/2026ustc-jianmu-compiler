define i32 @main() {
entry:
  %a = alloca float
  store float 0x40163851e0000000, float* %a
  %value = load float, float* %a
  %condition = fcmp ogt float %value, 0x3ff0000000000000
  br i1 %condition, label %yes, label %no

yes:
  ret i32 233

no:
  ret i32 0
}
