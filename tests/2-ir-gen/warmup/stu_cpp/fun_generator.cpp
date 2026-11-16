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
    auto *i32 = module.get_int32_type();
    auto *callee = Function::create(FunctionType::get(i32, {i32}), "callee", &module);
    builder.set_insert_point(BasicBlock::create(&module, "callee.entry", callee));
    auto *parameter = &*callee->get_args().begin();
    builder.create_ret(builder.create_imul(ConstantInt::get(2, &module), parameter));

    auto *main_function = Function::create(FunctionType::get(i32, {}), "main", &module);
    builder.set_insert_point(BasicBlock::create(&module, "main.entry", main_function));
    builder.create_ret(builder.create_call(callee, {ConstantInt::get(110, &module)}));
    std::cout << module.print();
}
