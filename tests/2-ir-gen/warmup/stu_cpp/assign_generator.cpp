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
    auto *main_type = FunctionType::get(i32, {});
    auto *main_function = Function::create(main_type, "main", &module);
    builder.set_insert_point(BasicBlock::create(&module, "entry", main_function));

    auto *a = builder.create_alloca(ArrayType::get(i32, 10));
    auto *first = builder.create_gep(a, {ConstantInt::get(0, &module), ConstantInt::get(0, &module)});
    builder.create_store(ConstantInt::get(10, &module), first);
    auto *twice = builder.create_imul(builder.create_load(first), ConstantInt::get(2, &module));
    auto *second = builder.create_gep(a, {ConstantInt::get(0, &module), ConstantInt::get(1, &module)});
    builder.create_store(twice, second);
    builder.create_ret(builder.create_load(second));
    std::cout << module.print();
}
