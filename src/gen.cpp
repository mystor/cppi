#include "gen.h"
#include <unordered_map>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

struct Scope {
    Scope *parent;
    std::unordered_map<const char*, llvm::Value*> vars;
    std::unordered_map<const char*, llvm::Value*>::iterator find(const char * key) {
        // Am I doing iterators wrong? Who the fuck knows...
        auto found = vars.find(key);
        if (found == vars.end() && parent != NULL) {
            auto parent_found = parent->find(key);
            if (parent_found == parent->end()) {
                return end();
            } else {
                return parent_found;
            }
        }
        return found;
    }
    std::unordered_map<const char*, llvm::Value*>::iterator end() {
        return vars.end();
    }
};

struct GenState {
    llvm::LLVMContext &context;
    llvm::Module *module;
    llvm::IRBuilder<> &builder;
    Scope *scope;
};

llvm::Type *get_type(GenState *st, Type &ty) {
    if (ty.ident == intern("int")) {
        return llvm::Type::getInt32Ty(st->context);
    } else if (ty.ident == intern("void")) {
        return llvm::Type::getVoidTy(st->context);
    } else {
        assert(false && "Invalid Type");
    }
}

class StmtGen;
class ExprGen : public ExprVisitor {
    GenState *st;
public:
    llvm::Value *value = NULL;

    ExprGen(GenState *st) : st(st) {}

    virtual void visit(StringExpr *expr) {
        assert(false && "Not implemented yet");
    }
    virtual void visit(IntExpr *expr) {
        // TODO: Not all ints are 32 bits
        value = llvm::ConstantInt::get(llvm::IntegerType::get(st->context, 32), expr->value);
    }
    virtual void visit(IdentExpr *expr) {
        auto var = st->scope->find(expr->ident);
        if (var == st->scope->end()) {
            assert(false && "No such variable");
        }
        if (var->second->getType()->isFunctionTy()) {
            // Functions are special!
            value = var->second;
        } else {
            value = st->builder.CreateLoad(var->second, expr->ident);
        }
    }
    virtual void visit(CallExpr *expr) {
        ExprGen callee_eg(st);
        expr->callee->accept(callee_eg);
        auto callee = callee_eg.value;

        assert(callee->getType()->isFunctionTy());

        std::vector<llvm::Value *> args;
        for (auto &arg : expr->args) {
            ExprGen arg_eg(st);
            arg->accept(arg_eg);
            args.push_back(arg_eg.value);
        }

        value = st->builder.CreateCall(callee,
                                       args,
                                       "call_result");
    }
    virtual void visit(InfixExpr *expr) {
        ExprGen lhs_eg(st);
        expr->lhs->accept(lhs_eg);
        ExprGen rhs_eg(st);
        expr->rhs->accept(rhs_eg);

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = st->builder.CreateAdd(lhs_eg.value, rhs_eg.value, "add_result");
        } break;
        case OPERATION_MINUS: {
            value = st->builder.CreateSub(lhs_eg.value, rhs_eg.value, "sub_result");
        } break;
        case OPERATION_TIMES: {
            value = st->builder.CreateMul(lhs_eg.value, rhs_eg.value, "mul_result");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st->builder.CreateUDiv(lhs_eg.value, rhs_eg.value, "div_result");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st->builder.CreateURem(lhs_eg.value, rhs_eg.value, "mod_result");
        } break;
        }
    }
};

class StmtGen : public StmtVisitor {
    GenState *st;
public:
    StmtGen(GenState *st) : st(st) {}

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = st->builder.CreateAlloca(get_type(st, stmt->type),
                                               nullptr, stmt->name);
        // Generate the expression (TODO: Allow undefined functions)
        ExprGen egen(st);
        stmt->value->accept(egen);

        // Store that value in the alloca
        st->builder.CreateStore(egen.value, alloca);

        st->scope->vars.emplace(stmt->name, alloca);
    }
    virtual void visit(ExprStmt *stmt) {
        // Generate the expression and ignore the value result
        ExprGen egen(st);
        stmt->expr->accept(egen);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            ExprGen egen(st);
            stmt->value->accept(egen);
            st->builder.CreateRet(egen.value);
        } else {
            st->builder.CreateRetVoid();
        }
    }
    virtual void visit(EmptyStmt *) {/* no-op */}
};

class ItemGen : public ItemVisitor {
    GenState *st;
public:
    ItemGen(GenState *st) : st(st) {}

    virtual void visit(FunctionItem *item) {
        // Creating the function's prototype
        std::vector<llvm::Type *> arg_types;

        for (auto arg : item->arguments) {
            arg_types.push_back(get_type(st, arg.type));
        }

        auto ft = llvm::FunctionType::get(get_type(st, item->return_type),
                                                         arg_types, false);

        auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, item->name, st->module);

        if (fn->getName() != item->name) {
            // This declaration has already been made
            fn->eraseFromParent();
            fn = st->module->getFunction(item->name);

            // If we have already defined a body, then that's a problem!
            if (! fn->empty()) {
                std::cerr << "Error: Redefinition of function " << item->name << ".";
                assert(false && "Function Redefinition");
            }

            // Check if the argument list is the same
            // TODO: Implement
        }

        // Set argument names
        Scope body_scope = Scope();
        body_scope.parent = st->scope;

        unsigned idx = 0;
        for (auto ai = fn->arg_begin(); idx != item->arguments.size(); ++ai, ++idx) {
            auto name = item->arguments[idx].name;
            ai->setName(name);
            body_scope.vars.emplace(name, ai);
        }

        // Creating the function's body!
        auto bb = llvm::BasicBlock::Create(st->context, "entry", fn);
        st->builder.SetInsertPoint(bb);

        // Generate the body!
        st->scope = &body_scope;
        for (auto &stmt : item->body) {
            // Generate every statement!
            StmtGen sg(st);
            stmt->accept(sg);
        }
        st->scope = body_scope.parent;

        // TODO: Implicit return statements!
        llvm::verifyFunction(*fn);

        // Insert it into the global scope!
        st->scope->vars.emplace(item->name, fn);
    }

    virtual void visit(EmptyItem *) {/* no-op */}
};

llvm::Module *generate_module(std::vector<std::unique_ptr<Item>> &items) {
    llvm::LLVMContext &context = llvm::getGlobalContext();
    llvm::Module *module = new llvm::Module("MyModuleName", context);
    llvm::IRBuilder<> builder = llvm::IRBuilder<>(context);

    Scope global_scope;
    GenState st { context, module, builder, &global_scope };
    for (auto &item : items) {
        ItemGen gen(&st);
        item->accept(gen);
    }

    return module;
}
