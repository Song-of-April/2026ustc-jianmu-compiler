#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "IRBuilder.hpp"
#include "Module.hpp"
#include "Type.hpp"
#include <iostream>

int main() {
    Module module;
    IRBuilder builder(nullptr, &module);
    auto *main_function = Function::create(
        FunctionType::get(module.get_int32_type(), {}), "main", &module);
    auto *entry = BasicBlock::create(&module, "entry", main_function);
    auto *yes = BasicBlock::create(&module, "yes", main_function);
    auto *no = BasicBlock::create(&module, "no", main_function);
    builder.set_insert_point(entry);
    auto *a = builder.create_alloca(module.get_float_type());
    builder.create_store(ConstantFP::get(5.555f, &module), a);
    auto *condition = builder.create_fcmp_gt(builder.create_load(a), ConstantFP::get(1.0f, &module));
    builder.create_cond_br(condition, yes, no);
    builder.set_insert_point(yes);
    builder.create_ret(ConstantInt::get(233, &module));
    builder.set_insert_point(no);
    builder.create_ret(ConstantInt::get(0, &module));
    std::cout << module.print();
}
