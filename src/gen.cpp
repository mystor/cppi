#include "gen.h"

#include <llvm/IR/Verifier.h>

// TODO(michael): This is a mess, and should be factored into something sane
llvm::Type *get_type(Scope *scope, Type &ty) {
    auto type = scope->type(ty.ident);
    assert(type != NULL && "Type does not exist");
    return type;
}

void gen_item(Program &prgm, Item &item);

// Generate an if
llvm::Value *gen_if(FnGenState &st, Branch *branches, size_t count);

class ExprGen : public ExprVisitor {
    FnGenState &st;
public:
    llvm::Value *value = NULL;

    ExprGen(FnGenState &st) : st(st) {}

    virtual void visit(StringExpr *) {
        assert(false && "Not implemented yet");
    }
    virtual void visit(IntExpr *expr) {
        // TODO: Not all ints are 32 bits
        value = llvm::ConstantInt::get(llvm::IntegerType::get(st.prgm.context, 32), expr->value);
    }
    virtual void visit(BoolExpr *expr) {
        if (expr->value) {
            value = llvm::ConstantInt::getTrue(st.prgm.context);
        } else {
            value = llvm::ConstantInt::getFalse(st.prgm.context);
        }
    }
    virtual void visit(IdentExpr *expr) {
        auto var = st.scope->var(expr->ident);
        if (var == NULL) {
            auto codeunit = st.prgm.code_units.find(ProgIdent(expr->ident));
            if (codeunit == st.prgm.code_units.end()) {
                assert(false && "No such variable");
            }

            std::cout << "Getting variable " << expr->ident << "\n";

            st.prgm.build(&codeunit->second);
            var = st.scope->var(expr->ident);
            if (var == NULL) {
                assert(false && "No such variable");
            }
        }
        auto ty = var->getType();
        if (ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy()) {
            // Functions are special!
            value = var;
        } else {
            value = st.builder.CreateLoad(var, expr->ident.data);
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

        value = st.builder.CreateCall(callee, args, "call_result");
    }
    virtual void visit(InfixExpr *expr) {
        auto lhs = gen_expr(st, *expr->lhs);
        auto rhs = gen_expr(st, *expr->rhs);

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = st.builder.CreateAdd(lhs, rhs, "add_result");
        } break;
        case OPERATION_MINUS: {
            value = st.builder.CreateSub(lhs, rhs, "sub_result");
        } break;
        case OPERATION_TIMES: {
            value = st.builder.CreateMul(lhs, rhs, "mul_result");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st.builder.CreateUDiv(lhs, rhs, "div_result");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st.builder.CreateURem(lhs, rhs, "mod_result");
        } break;
        }
    }
    virtual void visit(IfExpr *expr) {
        value = gen_if(st, expr->branches.data(), expr->branches.size());
    }
};



class StmtGen : public StmtVisitor {
    FnGenState &st;
public:
    StmtGen(FnGenState &st) : st(st) {}

    llvm::Value *value = NULL;

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = st.builder.CreateAlloca(get_type(st.scope, stmt->type),
                                               nullptr, stmt->name.data);

        // TODO: Allow undefined variables
        auto expr = gen_expr(st, *stmt->value);
        st.builder.CreateStore(expr, alloca);

        st.scope->push_var(stmt->name, alloca);

        value = NULL;
    }
    virtual void visit(ExprStmt *stmt) {
        value = gen_expr(st, *stmt->expr);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            st.builder.CreateRet(gen_expr(st, *stmt->value));
        } else {
            st.builder.CreateRetVoid();
        }

        value = NULL;
    }
    virtual void visit(EmptyStmt *) {
        value = NULL;
    }
};

llvm::Value *gen_if(FnGenState &st, Branch *branches, size_t count) {
    if (count > 0) {
        if (branches[0].cond != NULL) {
            auto cons = llvm::BasicBlock::Create(st.prgm.context, "if_cons", st.fn);
            auto alt = llvm::BasicBlock::Create(st.prgm.context, "if_alt", st.fn);
            auto after = llvm::BasicBlock::Create(st.prgm.context, "after_if", st.fn);

            auto cond = gen_expr(st, *branches[0].cond);

            st.builder.CreateCondBr(cond, cons, alt);

            // Generate the body
            st.builder.SetInsertPoint(cons);
            llvm::Value *cons_val = NULL;
            for (auto &stmt : branches[0].body) {
                cons_val = gen_stmt(st, *stmt);
            }
            st.builder.CreateBr(after);

            // Generate the else expression
            st.builder.SetInsertPoint(alt);
            llvm::Value *alt_val = gen_if(st, branches + 1, count - 1); // TODO: Eww, pointer math
            st.builder.CreateBr(after);

            // Generate the after block
            st.builder.SetInsertPoint(after);
            if (cons_val != NULL && alt_val != NULL) {
                assert(cons_val->getType() == alt_val->getType() && "Cons val and Alt val need the same type");
                auto phi_node = st.builder.CreatePHI(cons_val->getType(), 2, "if_value");
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

inline llvm::Value *gen_expr(FnGenState &st, Expr &expr) {
    ExprGen eg(st);
    expr.accept(eg);

    if (eg.value == NULL)
        std::cerr << "** NULL: " << expr << "\n";
    return eg.value;
}

inline llvm::Value *gen_stmt(FnGenState &st, Stmt &stmt) {
    StmtGen sg(st);
    stmt.accept(sg);
    return sg.value;
}
