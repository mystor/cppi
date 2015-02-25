#include "gen.h"

#include <llvm/IR/Verifier.h>

struct STVThing : public ValueThing {
    llvm::Value *value;
    TypeThing *type;

    STVThing(Program &, llvm::Value *value, TypeThing *type) : value(value), type(type) {};

    ValueThing *asValue() { return this; }

    llvm::Value *llValue() {
        return value;
    }

    TypeThing *typeOf() {
        return type;
    }

    void print(llvm::raw_ostream &os) {
        os << "STVThing(" << *value << "): ";
        type->print(os);
    }
};

void gen_item(Program &prgm, Item &item);

// Generate an if
ValueThing *gen_if(FnGenState &st, Branch *branches, size_t count);

class ExprGen : public ExprVisitor {
    FnGenState &st;
public:
    ValueThing *thing = NULL;

    ExprGen(FnGenState &st) : st(st) {}

    virtual void visit(StringExpr *expr) {
        // Create the string literal as a global string pointer
        // This includes the `\0` at the end of the string literal, which
        // is OK for us, because we want our code to support sending data
        // (like strings) to c programs easily

        auto data = st.builder.CreateGlobalStringPtr(expr->value.data, "string_literal");
        auto length = llvm::ConstantInt::get(llvm::IntegerType::get(st.prgm.context, st.prgm.pointer_width), expr->value.length);

        // TODO(michael): This should internally probably use the generic slice type,
        // once we get to that point in terms of compiler construction
        llvm::StructType *string_ty = llvm::cast<llvm::StructType>(st.prgm.builtin.string->asType()->llType());

        // Allocate the string struct, and fill it with data - This allocal _should_ be optimized
        // out by the compiler at some point
        auto string_struct = st.builder.CreateAlloca(string_ty);
        auto string_data_ptr = st.builder.CreateConstInBoundsGEP2_32(string_struct, 0, 0);
        auto string_len_ptr = st.builder.CreateConstInBoundsGEP2_32(string_struct, 0, 1);

        st.builder.CreateStore(data, string_data_ptr);
        st.builder.CreateStore(length, string_len_ptr);

        // Get it back into value-form
        thing = st.prgm.thing<STVThing>(st.builder.CreateLoad(string_struct),
                                        st.prgm.builtin.string->asType())->asValue();
    }
    virtual void visit(IntExpr *expr) {
        // TODO: Not all ints are 32 bits
        thing = st.prgm.thing<STVThing>(llvm::ConstantInt::get(llvm::IntegerType::get(st.prgm.context, 32), expr->value),
                                        st.prgm.builtin.i32->asType())->asValue();
    }
    virtual void visit(BoolExpr *expr) {
        if (expr->value) {
            thing = st.prgm.thing<STVThing>(llvm::ConstantInt::getTrue(st.prgm.context),
                                            st.prgm.builtin.boolean->asType())->asValue();
        } else {
            thing = st.prgm.thing<STVThing>(llvm::ConstantInt::getFalse(st.prgm.context),
                                            st.prgm.builtin.boolean->asType())->asValue();
        }
    }
    virtual void visit(IdentExpr *expr) {
        Thing *AThing = st.scope->thing(expr->ident);

        assert(AThing != NULL);

        thing = AThing->asValue();
        assert(AThing->asValue());
    }
    virtual void visit(CallExpr *expr) {
        auto callee = gen_expr(st, *expr->callee)->asValue()->llValue();

        /*
        auto ty = callee->getType();
        assert(ty->isPointerTy() && ty->getPointerElementType()->isFunctionTy());
        */
        // TODO(michael): assert(callee->isCallable());

        std::vector<llvm::Value *> args;
        for (auto &arg : expr->args) {
            auto earg = gen_expr(st, *arg)->asValue();
            assert(earg != NULL);

            // TODO(michael): Check types match that of function call

            args.push_back(earg->llValue());
        }

        thing = st.prgm.thing<STVThing>(st.builder.CreateCall(callee, args, "call_result"),
                                        (TypeThing *)NULL /* TODO IMPLEMENT */);
    }
    virtual void visit(InfixExpr *expr) {
        auto lhs = gen_expr(st, *expr->lhs)->asValue();
        auto rhs = gen_expr(st, *expr->rhs)->asValue();

        llvm::Value *value;

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = st.builder.CreateAdd(lhs->llValue(), rhs->llValue(), "add_result");
        } break;
        case OPERATION_MINUS: {
            value = st.builder.CreateSub(lhs->llValue(), rhs->llValue(), "sub_result");
        } break;
        case OPERATION_TIMES: {
            value = st.builder.CreateMul(lhs->llValue(), rhs->llValue(), "mul_result");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st.builder.CreateUDiv(lhs->llValue(), rhs->llValue(), "div_result");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st.builder.CreateURem(lhs->llValue(), rhs->llValue(), "mod_result");
        } break;
        }
    }
    virtual void visit(IfExpr *expr) {
        thing = gen_if(st, expr->branches.data(), expr->branches.size());
    }
};



class StmtGen : public StmtVisitor {
    FnGenState &st;
public:
    StmtGen(FnGenState &st) : st(st) {}

    ValueThing *thing = NULL;

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = st.builder.CreateAlloca(st.prgm.GetType(st.scope, stmt->type)->llType(),
                                               nullptr, stmt->name.data);

        // TODO: Allow undefined variables
        auto expr = gen_expr(st, *stmt->value);
        std::cout << "HERE" << "\n";
        st.builder.CreateStore(expr->llValue(), alloca);
        std::cout << "THERE" << "\n";

        st.scope->push_thing(stmt->name,
                             st.prgm.thing<STVThing>(alloca, expr->typeOf()));

        thing = NULL;
    }
    virtual void visit(ExprStmt *stmt) {
        thing = gen_expr(st, *stmt->expr);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            st.builder.CreateRet(gen_expr(st, *stmt->value)->llValue());
        } else {
            st.builder.CreateRetVoid();
        }

        thing = NULL;
    }
    virtual void visit(EmptyStmt *) {
        thing = NULL;
    }
};

ValueThing *gen_if(FnGenState &st, Branch *branches, size_t count) {
    if (count > 0) {
        if (branches[0].cond != NULL) {
            auto cons = llvm::BasicBlock::Create(st.prgm.context, "if_cons", st.fn);
            auto alt = llvm::BasicBlock::Create(st.prgm.context, "if_alt", st.fn);
            auto after = llvm::BasicBlock::Create(st.prgm.context, "after_if", st.fn);

            auto cond = gen_expr(st, *branches[0].cond);

            st.builder.CreateCondBr(cond->llValue(), cons, alt);

            // Generate the body
            st.builder.SetInsertPoint(cons);
            ValueThing *cons_val = NULL;
            for (auto &stmt : branches[0].body) {
                cons_val = gen_stmt(st, *stmt);
            }
            st.builder.CreateBr(after);

            // Generate the else expression
            st.builder.SetInsertPoint(alt);
            ValueThing *alt_val = gen_if(st, branches + 1, count - 1); // TODO: Eww, pointer math
            st.builder.CreateBr(after);

            // Generate the after block
            st.builder.SetInsertPoint(after);
            if (cons_val != NULL && alt_val != NULL) {
                assert(cons_val->typeOf() == alt_val->typeOf() && "Cons val and Alt val need the same type");
                auto phi_node = st.builder.CreatePHI(cons_val->typeOf()->llType(), 2, "if_value");
                phi_node->addIncoming(cons_val->llValue(), cons);
                phi_node->addIncoming(alt_val->llValue(), alt);

                return st.prgm.thing<STVThing>(phi_node,
                                               cons_val->typeOf());
            } else {
                assert(cons_val == alt_val); // == NULL

                return NULL;
            }

        } else {
            // The else branch. It is unconditional
            ValueThing *cons_val = NULL;
            for (auto &stmt : branches[0].body) {
                cons_val = gen_stmt(st, *stmt);
            }
            return cons_val;
        }
    } else {
        return NULL;
    }
}

inline ValueThing *gen_expr(FnGenState &st, Expr &expr) {
    ExprGen eg(st);
    expr.accept(eg);

    // if (eg.thing == NULL)
    //     std::cerr << "** NULL: " << expr << "\n";
    return eg.thing;
}

inline ValueThing *gen_stmt(FnGenState &st, Stmt &stmt) {
    StmtGen sg(st);
    stmt.accept(sg);
    return sg.thing;
}
