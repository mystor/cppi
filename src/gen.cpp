#include "gen.h"
#include <unordered_map>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

struct Scope {
    Scope *parent = NULL;
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

    std::unordered_map<const char*, llvm::Type*> types;
};

struct GenState {
    llvm::LLVMContext &context;
    llvm::Module *module;
    llvm::IRBuilder<> &builder;
    llvm::Function *current_function;
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

// The functions for generating an item. These are defined at the bottom of the file
llvm::Value *gen_expr(GenState *st, Expr &expr);
llvm::Value *gen_stmt(GenState *st, Stmt &stmt);
void gen_item(GenState *st, Item &item);

// Generate an if
llvm::Value *gen_if(GenState *st, Branch *branches, size_t count);

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
    virtual void visit(BoolExpr *expr) {
        if (expr->value) {
            value = llvm::ConstantInt::getTrue(st->context);
        } else {
            value = llvm::ConstantInt::getFalse(st->context);
        }
    }
    virtual void visit(IdentExpr *expr) {
        auto var = st->scope->find(expr->ident);
        if (var == st->scope->end()) {
            assert(false && "No such variable");
        }
        auto ty = var->second->getType();
        if (ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy()) {
            // Functions are special!
            value = var->second;
        } else {
            value = st->builder.CreateLoad(var->second, expr->ident);
        }
    }
    virtual void visit(CallExpr *expr) {
        auto callee = gen_expr(st, *expr->callee);

        auto ty = callee->getType();
        assert(ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy());

        std::vector<llvm::Value *> args;
        for (auto &arg : expr->args) {
            args.push_back(gen_expr(st, *arg));
        }

        value = st->builder.CreateCall(callee, args, "call_result");
    }
    virtual void visit(InfixExpr *expr) {
        auto lhs = gen_expr(st, *expr->lhs);
        auto rhs = gen_expr(st, *expr->rhs);

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = st->builder.CreateAdd(lhs, rhs, "add_result");
        } break;
        case OPERATION_MINUS: {
            value = st->builder.CreateSub(lhs, rhs, "sub_result");
        } break;
        case OPERATION_TIMES: {
            value = st->builder.CreateMul(lhs, rhs, "mul_result");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st->builder.CreateUDiv(lhs, rhs, "div_result");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st->builder.CreateURem(lhs, rhs, "mod_result");
        } break;
        }
    }
    virtual void visit(IfExpr *expr) {
        value = gen_if(st, expr->branches.data(), expr->branches.size());
    }
};



class StmtGen : public StmtVisitor {
    GenState *st;
public:
    StmtGen(GenState *st) : st(st) {}

    llvm::Value *value = NULL;

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = st->builder.CreateAlloca(get_type(st, stmt->type),
                                               nullptr, stmt->name);

        // TODO: Allow undefined variables
        auto expr = gen_expr(st, *stmt->value);
        st->builder.CreateStore(expr, alloca);

        st->scope->vars.emplace(stmt->name, alloca);

        value = NULL;
    }
    virtual void visit(ExprStmt *stmt) {
        value = gen_expr(st, *stmt->expr);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            st->builder.CreateRet(gen_expr(st, *stmt->value));
        } else {
            st->builder.CreateRetVoid();
        }

        value = NULL;
    }
    virtual void visit(EmptyStmt *) {
        value = NULL;
    }
};

llvm::Function *generate_function_proto(GenState *st, FunctionProto &proto) {
    // Creating the function's prototype
    std::vector<llvm::Type *> arg_types;

    for (auto arg : proto.arguments) {
        arg_types.push_back(get_type(st, arg.type));
    }

    auto ft = llvm::FunctionType::get(get_type(st, proto.return_type),
                                      arg_types, false);

    auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, proto.name, st->module);

    if (fn->getName() != proto.name) {
        // This declaration has already been made
        fn->eraseFromParent();
        fn = st->module->getFunction(proto.name);

        // If we have already defined a body, then that's a problem!
        if (! fn->empty()) {
            std::cerr << "Error: Redefinition of function " << proto.name << ".";
            assert(false && "Function Redefinition");
        }

        // Check if the argument list is the same
        // TODO: Implement
    }

    // Set argument names
    unsigned idx = 0;
    for (auto ai = fn->arg_begin(); idx != proto.arguments.size(); ++ai, ++idx) {
        auto name = proto.arguments[idx].name;
        ai->setName(name);
    }

    return fn;
}

class ItemGen : public ItemVisitor {
    GenState *st;
public:
    ItemGen(GenState *st) : st(st) {}

    virtual void visit(FunctionItem *item) {
        auto fn = generate_function_proto(st, item->proto);

        // Set argument names
        Scope body_scope = Scope();
        body_scope.parent = st->scope;

        unsigned idx = 0;
        for (auto ai = fn->arg_begin(); idx != item->proto.arguments.size(); ++ai, ++idx) {
            auto name = item->proto.arguments[idx].name;
            body_scope.vars.emplace(name, ai);
        }

        // Creating the function's body!
        auto bb = llvm::BasicBlock::Create(st->context, "entry", fn);
        st->builder.SetInsertPoint(bb);
        st->current_function = fn;

        // Generate the body!
        st->scope = &body_scope;
        for (auto &stmt : item->body) {
            gen_stmt(st, *stmt);
        }
        st->scope = body_scope.parent;
        st->current_function = NULL;

        // TODO: Implicit return statements!
        llvm::verifyFunction(*fn);

        // Insert it into the global scope!
        st->scope->vars.emplace(item->proto.name, fn);
    }

    virtual void visit(StructItem *item) {
        assert(false && "Not implemented yet");
    }

    virtual void visit(FFIFunctionItem *item) {
        auto fn = generate_function_proto(st, item->proto);
        st->scope->vars.emplace(item->proto.name, fn);
    }

    virtual void visit(EmptyItem *) {/* no-op */}
};



llvm::Value *gen_if(GenState *st, Branch *branches, size_t count) {
    if (count > 0) {
        if (branches[0].cond != NULL) {
            auto cons = llvm::BasicBlock::Create(st->context, "if_cons", st->current_function);
            auto alt = llvm::BasicBlock::Create(st->context, "if_alt", st->current_function);
            auto after = llvm::BasicBlock::Create(st->context, "after_if", st->current_function);

            auto cond = gen_expr(st, *branches[0].cond);

            st->builder.CreateCondBr(cond, cons, alt);

            // Generate the body
            st->builder.SetInsertPoint(cons);
            llvm::Value *cons_val = NULL;
            for (auto &stmt : branches[0].body) {
                cons_val = gen_stmt(st, *stmt);
            }
            st->builder.CreateBr(after);

            // Generate the else expression
            st->builder.SetInsertPoint(alt);
            llvm::Value *alt_val = gen_if(st, branches + 1, count - 1); // TODO: Eww, pointer math
            st->builder.CreateBr(after);

            // Generate the after block
            st->builder.SetInsertPoint(after);
            if (cons_val != NULL && alt_val != NULL) {
                assert(cons_val->getType() == alt_val->getType() && "Cons val and Alt val need the same type");
                auto phi_node = st->builder.CreatePHI(cons_val->getType(), 2, "if_value");
                phi_node->addIncoming(cons_val, cons);
                phi_node->addIncoming(alt_val, alt);

                return phi_node;
            } else {
                assert(cons_val == alt_val); // == NULL

                return NULL;
            }

        } else {
            // The else branch. It is unconditional
            llvm::Value *cons_val = NULL;
            for (auto &stmt : branches[0].body) {
                cons_val = gen_stmt(st, *stmt);
            }
            return cons_val;
        }
    } else {
        return NULL;
    }
}

inline llvm::Value *gen_expr(GenState *st, Expr &expr) {
    ExprGen eg(st);
    expr.accept(eg);
    return eg.value;
}

inline llvm::Value *gen_stmt(GenState *st, Stmt &stmt) {
    StmtGen sg(st);
    stmt.accept(sg);
    return sg.value;
}

inline void gen_item(GenState *st, Item &item) {
    ItemGen ig(st);
    item.accept(ig);
}

llvm::Module *generate_module(std::vector<std::unique_ptr<Item>> &items) {
    llvm::LLVMContext &context = llvm::getGlobalContext();
    llvm::Module *module = new llvm::Module("cppi_module", context);
    llvm::IRBuilder<> builder = llvm::IRBuilder<>(context);

    Scope global_scope;
    GenState st { context, module, builder, NULL, &global_scope };
    for (auto &item : items) {
        gen_item(&st, *item);
    }

    return module;
}
