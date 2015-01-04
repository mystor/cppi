#include "gen.h"

#include <llvm/IR/Verifier.h>

// TODO(michael): This is a mess, and should be factored into something sane
llvm::Type *get_type(Program &prgm, Type &ty) {
    if (ty.ident == intern("int")) {
        return llvm::Type::getInt32Ty(prgm.context);
    } else if (ty.ident == intern("void")) {
        return llvm::Type::getVoidTy(prgm.context);
    } else {
        assert(false && "Invalid Type");
    }
}

void gen_item(Program &prgm, Item &item);

// Generate an if
llvm::Value *gen_if(Program &prgm, Branch *branches, size_t count);

class ExprGen : public ExprVisitor {
    Program &prgm;
public:
    llvm::Value *value = NULL;

    ExprGen(Program &prgm) : prgm(prgm) {}

    virtual void visit(StringExpr *) {
        assert(false && "Not implemented yet");
    }
    virtual void visit(IntExpr *expr) {
        // TODO: Not all ints are 32 bits
        value = llvm::ConstantInt::get(llvm::IntegerType::get(prgm.context, 32), expr->value);
    }
    virtual void visit(BoolExpr *expr) {
        if (expr->value) {
            value = llvm::ConstantInt::getTrue(prgm.context);
        } else {
            value = llvm::ConstantInt::getFalse(prgm.context);
        }
    }
    virtual void visit(IdentExpr *expr) {
        auto var = prgm.scope.vars.find(expr->ident);
        if (var == prgm.scope.vars.end()) {
            // Try to build it
            auto codeunit = prgm.code_units.find(ProgIdent(expr->ident));
            if (codeunit == prgm.code_units.end()) {
                assert(false && "No such variable");
            }

            prgm.build(&codeunit->second);
            var = prgm.scope.vars.find(expr->ident);
            if (var == prgm.scope.vars.end()) {
                assert(false && "No such variable");
            }
        }
        auto ty = var->second->getType();
        if (ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy()) {
            // Functions are special!
            value = var->second;
        } else {
            value = prgm.builder.CreateLoad(var->second, expr->ident.data);
        }
    }
    virtual void visit(CallExpr *expr) {
        auto callee = gen_expr(prgm, *expr->callee);

        auto ty = callee->getType();
        assert(ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy());

        std::vector<llvm::Value *> args;
        for (auto &arg : expr->args) {
            args.push_back(gen_expr(prgm, *arg));
        }

        value = prgm.builder.CreateCall(callee, args, "call_result");
    }
    virtual void visit(InfixExpr *expr) {
        auto lhs = gen_expr(prgm, *expr->lhs);
        auto rhs = gen_expr(prgm, *expr->rhs);

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = prgm.builder.CreateAdd(lhs, rhs, "add_result");
        } break;
        case OPERATION_MINUS: {
            value = prgm.builder.CreateSub(lhs, rhs, "sub_result");
        } break;
        case OPERATION_TIMES: {
            value = prgm.builder.CreateMul(lhs, rhs, "mul_result");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = prgm.builder.CreateUDiv(lhs, rhs, "div_result");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = prgm.builder.CreateURem(lhs, rhs, "mod_result");
        } break;
        }
    }
    virtual void visit(IfExpr *expr) {
        value = gen_if(prgm, expr->branches.data(), expr->branches.size());
    }
};



class StmtGen : public StmtVisitor {
    Program &prgm;
public:
    StmtGen(Program &prgm) : prgm(prgm) {}

    llvm::Value *value = NULL;

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = prgm.builder.CreateAlloca(get_type(prgm, stmt->type),
                                               nullptr, stmt->name.data);

        // TODO: Allow undefined variables
        auto expr = gen_expr(prgm, *stmt->value);
        prgm.builder.CreateStore(expr, alloca);

        prgm.scope.vars.emplace(stmt->name, alloca);

        value = NULL;
    }
    virtual void visit(ExprStmt *stmt) {
        value = gen_expr(prgm, *stmt->expr);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            prgm.builder.CreateRet(gen_expr(prgm, *stmt->value));
        } else {
            prgm.builder.CreateRetVoid();
        }

        value = NULL;
    }
    virtual void visit(EmptyStmt *) {
        value = NULL;
    }
};

llvm::Function *generate_function_proto(Program &prgm, FunctionProto &proto) {
    // Creating the function's prototype
    std::vector<llvm::Type *> arg_types;

    for (auto arg : proto.arguments) {
        arg_types.push_back(get_type(prgm, arg.type));
    }

    auto ft = llvm::FunctionType::get(get_type(prgm, proto.return_type),
                                      arg_types, false);

    auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, proto.name.data, prgm.module);

    if (fn->getName() != proto.name.data) {
        // This declaration has already been made
        fn->eraseFromParent();
        fn = prgm.module->getFunction(proto.name.data);

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
        ai->setName(name.data);
    }

    return fn;
}

llvm::Value *gen_if(Program &prgm, Branch *branches, size_t count) {
    if (count > 0) {
        if (branches[0].cond != NULL) {
            auto cons = llvm::BasicBlock::Create(prgm.context, "if_cons", prgm.current_function);
            auto alt = llvm::BasicBlock::Create(prgm.context, "if_alt", prgm.current_function);
            auto after = llvm::BasicBlock::Create(prgm.context, "after_if", prgm.current_function);

            auto cond = gen_expr(prgm, *branches[0].cond);

            prgm.builder.CreateCondBr(cond, cons, alt);

            // Generate the body
            prgm.builder.SetInsertPoint(cons);
            llvm::Value *cons_val = NULL;
            for (auto &stmt : branches[0].body) {
                cons_val = gen_stmt(prgm, *stmt);
            }
            prgm.builder.CreateBr(after);

            // Generate the else expression
            prgm.builder.SetInsertPoint(alt);
            llvm::Value *alt_val = gen_if(prgm, branches + 1, count - 1); // TODO: Eww, pointer math
            prgm.builder.CreateBr(after);

            // Generate the after block
            prgm.builder.SetInsertPoint(after);
            if (cons_val != NULL && alt_val != NULL) {
                assert(cons_val->getType() == alt_val->getType() && "Cons val and Alt val need the same type");
                auto phi_node = prgm.builder.CreatePHI(cons_val->getType(), 2, "if_value");
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
                cons_val = gen_stmt(prgm, *stmt);
            }
            return cons_val;
        }
    } else {
        return NULL;
    }
}

inline llvm::Value *gen_expr(Program &prgm, Expr &expr) {
    ExprGen eg(prgm);
    expr.accept(eg);
    return eg.value;
}

inline llvm::Value *gen_stmt(Program &prgm, Stmt &stmt) {
    StmtGen sg(prgm);
    stmt.accept(sg);
    return sg.value;
}
