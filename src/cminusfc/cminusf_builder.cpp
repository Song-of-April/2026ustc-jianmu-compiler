#include "cminusf_builder.hpp"
#include "GlobalVariable.hpp"

#include <cassert>

#define CI(n) ConstantInt::get(n, module.get())
#define CF(n) ConstantFP::get(n, module.get())

Value *CminusfBuilder::cast_to(Value *value, Type *type) {
    assert(value && type);
    auto *from = value->get_type();
    if (from == type) return value;
    if (from->is_int1_type() && type->is_int32_type())
        return builder->create_zext(value, type);
    if (from->is_int1_type() && type->is_float_type())
        return builder->create_sitofp(builder->create_zext(value, module->get_int32_type()), type);
    if (from->is_int32_type() && type->is_float_type())
        return builder->create_sitofp(value, type);
    if (from->is_float_type() && type->is_int32_type())
        return builder->create_fptosi(value, type);
    assert(false && "unsupported conversion");
    return nullptr;
}

Value *CminusfBuilder::truth_value(Value *value) {
    if (value->get_type()->is_int1_type()) return value;
    if (value->get_type()->is_float_type()) return builder->create_fcmp_ne(value, CF(0));
    return builder->create_icmp_ne(value, CI(0));
}

Value *CminusfBuilder::visit(ASTProgram &node) {
    Value *result = nullptr;
    for (auto &declaration : node.declarations) result = declaration->accept(*this);
    return result;
}

Value *CminusfBuilder::visit(ASTNum &node) {
    return node.type == TYPE_FLOAT ? static_cast<Value *>(CF(node.f_val))
                                   : static_cast<Value *>(CI(node.i_val));
}

Value *CminusfBuilder::visit(ASTVarDeclaration &node) {
    Type *type = node.type == TYPE_FLOAT ? static_cast<Type *>(module->get_float_type()) : module->get_int32_type();
    if (node.num) type = ArrayType::get(type, node.num->i_val);
    Value *slot;
    if (scope.in_global())
        slot = GlobalVariable::create(node.id, module.get(), type, false,
                                      ConstantZero::get(type, module.get()));
    else {
        auto *current = builder->get_insert_block();
        builder->set_insert_point(context.func->get_entry_block());
        slot = builder->create_alloca_begin(type);
        builder->set_insert_point(current);
    }
    bool inserted = scope.push(node.id, slot);
    assert(inserted);
    return slot;
}

Value *CminusfBuilder::visit(ASTFunDeclaration &node) {
    Type *ret = node.type == TYPE_FLOAT ? module->get_float_type()
               : node.type == TYPE_INT ? module->get_int32_type() : module->get_void_type();
    std::vector<Type *> parameter_types;
    for (auto &parameter : node.params) {
        Type *type = parameter->type == TYPE_FLOAT ? static_cast<Type *>(module->get_float_type()) : module->get_int32_type();
        parameter_types.push_back(parameter->isarray ? PointerType::get(type) : type);
    }
    auto *function = Function::create(FunctionType::get(ret, parameter_types), node.id, module.get());
    bool inserted = scope.push(node.id, function);
    assert(inserted);
    context.func = function;
    builder->set_insert_point(BasicBlock::create(module.get(), "entry", function));
    scope.enter();
    auto arg = function->get_args().begin();
    for (auto &parameter : node.params) {
        auto *slot = builder->create_alloca_begin(arg->get_type());
        builder->create_store(&*arg, slot);
        bool inserted_parameter = scope.push(parameter->id, slot);
        assert(inserted_parameter);
        ++arg;
    }
    context.function_body = true;
    node.compound_stmt->accept(*this);
    if (!builder->get_insert_block()->is_terminated()) {
        if (ret->is_void_type()) builder->create_void_ret();
        else builder->create_ret(ret->is_float_type() ? static_cast<Value *>(CF(0))
                                                        : static_cast<Value *>(CI(0)));
    }
    scope.exit();
    context.func = nullptr;
    return function;
}

Value *CminusfBuilder::visit(ASTParam &) { return nullptr; }

Value *CminusfBuilder::visit(ASTCompoundStmt &node) {
    bool is_function_body = context.function_body;
    context.function_body = false;
    if (!is_function_body) scope.enter();
    for (auto &declaration : node.local_declarations) declaration->accept(*this);
    for (auto &statement : node.statement_list) {
        if (builder->get_insert_block()->is_terminated()) break;
        statement->accept(*this);
    }
    if (!is_function_body) scope.exit();
    return nullptr;
}

Value *CminusfBuilder::visit(ASTExpressionStmt &node) {
    return node.expression ? node.expression->accept(*this) : nullptr;
}

Value *CminusfBuilder::visit(ASTSelectionStmt &node) {
    Value *condition = truth_value(node.expression->accept(*this));
    auto *yes = BasicBlock::create(module.get(), "", context.func);
    auto *end = BasicBlock::create(module.get(), "", context.func);
    auto *no = node.else_statement ? BasicBlock::create(module.get(), "", context.func) : end;
    builder->create_cond_br(condition, yes, no);
    builder->set_insert_point(yes);
    node.if_statement->accept(*this);
    if (!builder->get_insert_block()->is_terminated()) builder->create_br(end);
    if (node.else_statement) {
        builder->set_insert_point(no);
        node.else_statement->accept(*this);
        if (!builder->get_insert_block()->is_terminated()) builder->create_br(end);
    }
    builder->set_insert_point(end);
    return nullptr;
}

Value *CminusfBuilder::visit(ASTIterationStmt &node) {
    auto *condition = BasicBlock::create(module.get(), "", context.func);
    auto *body = BasicBlock::create(module.get(), "", context.func);
    auto *end = BasicBlock::create(module.get(), "", context.func);
    builder->create_br(condition);
    builder->set_insert_point(condition);
    builder->create_cond_br(truth_value(node.expression->accept(*this)), body, end);
    builder->set_insert_point(body);
    node.statement->accept(*this);
    if (!builder->get_insert_block()->is_terminated()) builder->create_br(condition);
    builder->set_insert_point(end);
    return nullptr;
}

Value *CminusfBuilder::visit(ASTReturnStmt &node) {
    if (!node.expression) return builder->create_void_ret();
    auto *value = node.expression->accept(*this);
    return builder->create_ret(cast_to(value, context.func->get_return_type()));
}

Value *CminusfBuilder::array_element(ASTVar &node, Value *binding) {
    auto *pointee = binding->get_type()->get_pointer_element_type();
    Value *base = pointee->is_pointer_type() ? builder->create_load(binding) : binding;
    bool outer_wants_address = context.want_address;
    context.want_address = false;
    Value *index = cast_to(node.expression->accept(*this), module->get_int32_type());
    context.want_address = outer_wants_address;
    auto *negative = builder->create_icmp_lt(index, CI(0));
    auto *bad = BasicBlock::create(module.get(), "", context.func);
    auto *good = BasicBlock::create(module.get(), "", context.func);
    builder->create_cond_br(negative, bad, good);
    builder->set_insert_point(bad);
    builder->create_call(scope.find("neg_idx_except"), {});
    builder->create_br(good);
    builder->set_insert_point(good);
    if (pointee->is_array_type()) return builder->create_gep(base, {CI(0), index});
    return builder->create_gep(base, {index});
}

Value *CminusfBuilder::visit(ASTVar &node) {
    auto *binding = scope.find(node.id);
    if (node.expression) {
        auto *address = array_element(node, binding);
        return context.want_address ? address : builder->create_load(address);
    }
    Type *pointee = binding->get_type()->get_pointer_element_type();
    if (pointee->is_array_type()) return builder->create_gep(binding, {CI(0), CI(0)});
    if (pointee->is_pointer_type()) return builder->create_load(binding);
    return context.want_address ? binding : builder->create_load(binding);
}

Value *CminusfBuilder::visit(ASTAssignExpression &node) {
    context.want_address = true;
    auto *address = node.var->accept(*this);
    context.want_address = false;
    auto *value = cast_to(node.expression->accept(*this), address->get_type()->get_pointer_element_type());
    builder->create_store(value, address);
    return value;
}

Value *CminusfBuilder::visit(ASTSimpleExpression &node) {
    auto *lhs = node.additive_expression_l->accept(*this);
    if (!node.additive_expression_r) return lhs;
    auto *rhs = node.additive_expression_r->accept(*this);
    bool floating = lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type();
    Type *type = floating ? static_cast<Type *>(module->get_float_type()) : module->get_int32_type();
    lhs = cast_to(lhs, type);
    rhs = cast_to(rhs, type);
    Value *result = nullptr;
    if (floating) {
        switch (node.op) {
        case OP_LE: result = builder->create_fcmp_le(lhs, rhs); break;
        case OP_LT: result = builder->create_fcmp_lt(lhs, rhs); break;
        case OP_GT: result = builder->create_fcmp_gt(lhs, rhs); break;
        case OP_GE: result = builder->create_fcmp_ge(lhs, rhs); break;
        case OP_EQ: result = builder->create_fcmp_eq(lhs, rhs); break;
        case OP_NEQ: result = builder->create_fcmp_ne(lhs, rhs); break;
        }
    } else {
        switch (node.op) {
        case OP_LE: result = builder->create_icmp_le(lhs, rhs); break;
        case OP_LT: result = builder->create_icmp_lt(lhs, rhs); break;
        case OP_GT: result = builder->create_icmp_gt(lhs, rhs); break;
        case OP_GE: result = builder->create_icmp_ge(lhs, rhs); break;
        case OP_EQ: result = builder->create_icmp_eq(lhs, rhs); break;
        case OP_NEQ: result = builder->create_icmp_ne(lhs, rhs); break;
        }
    }
    return builder->create_zext(result, module->get_int32_type());
}

Value *CminusfBuilder::visit(ASTAdditiveExpression &node) {
    if (!node.additive_expression) return node.term->accept(*this);
    auto *lhs = node.additive_expression->accept(*this);
    auto *rhs = node.term->accept(*this);
    bool floating = lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type();
    Type *type = floating ? static_cast<Type *>(module->get_float_type()) : module->get_int32_type();
    lhs = cast_to(lhs, type);
    rhs = cast_to(rhs, type);
    if (floating) return node.op == OP_PLUS ? static_cast<Value *>(builder->create_fadd(lhs, rhs))
                                             : static_cast<Value *>(builder->create_fsub(lhs, rhs));
    return node.op == OP_PLUS ? static_cast<Value *>(builder->create_iadd(lhs, rhs))
                              : static_cast<Value *>(builder->create_isub(lhs, rhs));
}

Value *CminusfBuilder::visit(ASTTerm &node) {
    if (!node.term) return node.factor->accept(*this);
    auto *lhs = node.term->accept(*this);
    auto *rhs = node.factor->accept(*this);
    bool floating = lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type();
    Type *type = floating ? static_cast<Type *>(module->get_float_type()) : module->get_int32_type();
    lhs = cast_to(lhs, type);
    rhs = cast_to(rhs, type);
    if (floating) return node.op == OP_MUL ? static_cast<Value *>(builder->create_fmul(lhs, rhs))
                                            : static_cast<Value *>(builder->create_fdiv(lhs, rhs));
    return node.op == OP_MUL ? static_cast<Value *>(builder->create_imul(lhs, rhs))
                             : static_cast<Value *>(builder->create_isdiv(lhs, rhs));
}

Value *CminusfBuilder::visit(ASTCall &node) {
    auto *function = static_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    auto *type = function->get_function_type();
    for (size_t i = 0; i < node.args.size(); ++i) {
        auto *arg = node.args[i]->accept(*this);
        args.push_back(cast_to(arg, type->get_param_type(i)));
    }
    return builder->create_call(function, args);
}
