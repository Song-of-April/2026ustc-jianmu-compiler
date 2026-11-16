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
    auto *test = BasicBlock::create(&module, "test", main_function);
    auto *body = BasicBlock::create(&module, "body", main_function);
    auto *done = BasicBlock::create(&module, "done", main_function);
    builder.set_insert_point(entry);
    auto *a = builder.create_alloca(module.get_int32_type());
    auto *i = builder.create_alloca(module.get_int32_type());
    builder.create_store(ConstantInt::get(10, &module), a);
    builder.create_store(ConstantInt::get(0, &module), i);
    builder.create_br(test);
    builder.set_insert_point(test);
    auto *current_i = builder.create_load(i);
    auto *condition = builder.create_icmp_lt(current_i, ConstantInt::get(10, &module));
    builder.create_cond_br(condition, body, done);
    builder.set_insert_point(body);
    auto *next_i = builder.create_iadd(current_i, ConstantInt::get(1, &module));
    builder.create_store(next_i, i);
    builder.create_store(builder.create_iadd(builder.create_load(a), next_i), a);
    builder.create_br(test);
    builder.set_insert_point(done);
    builder.create_ret(builder.create_load(a));
    std::cout << module.print();
}
