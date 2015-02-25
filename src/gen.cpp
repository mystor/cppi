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

void genItem(Program &prgm, Item &item);

// Generate an if
ValueThing *genIf(FnGenState &st, Branch *branches, size_t count);

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

        auto data = st.builder.CreateGlobalStringPtr(expr->value.data, "stringLiteral");
        auto length = llvm::ConstantInt::get(llvm::IntegerType::get(st.prgm.context, st.prgm.pointerWidth), expr->value.length);

        // TODO(michael): This should internally probably use the generic slice type,
        // once we get to that point in terms of compiler construction
        llvm::StructType *stringTy = llvm::cast<llvm::StructType>(st.prgm.builtin.string->asType()->llType());

        // Allocate the string struct, and fill it with data - This allocal Should_ be optimized
        // out by the compiler at some point
        auto stringStruct = st.builder.CreateAlloca(stringTy);
        auto stringDataPtr = st.builder.CreateConstInBoundsGEP2_32(stringStruct, 0, 0);
        auto stringLenPtr = st.builder.CreateConstInBoundsGEP2_32(stringStruct, 0, 1);

        st.builder.CreateStore(data, stringDataPtr);
        st.builder.CreateStore(length, stringLenPtr);

        // Get it back into value-form
        thing = st.prgm.thing<STVThing>(st.builder.CreateLoad(stringStruct),
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
    virtual void visit(MkExpr *expr) {
        assert(false && "Unimplemented");
    }
    virtual void visit(IdentExpr *expr) {
        Thing *AThing = st.scope->thing(expr->ident);

        assert(AThing != NULL);

        thing = AThing->asValue();
        assert(AThing->asValue());
    }
    virtual void visit(CallExpr *expr) {
        auto callee = genExpr(st, *expr->callee)->asValue()->llValue();

        // TODO(michael): assert(callee->isCallable());

        std::vector<llvm::Value *> args;
        for (auto &arg : expr->args) {
            auto earg = genExpr(st, *arg)->asValue();
            assert(earg != NULL);

            // TODO(michael): Check types match that of function call

            args.push_back(earg->llValue());
        }

        thing = st.prgm.thing<STVThing>(st.builder.CreateCall(callee, args, "callResult"),
                                        (TypeThing *)NULL /* TODO IMPLEMENT */);
    }
    virtual void visit(MthdCallExpr *expr) {
        assert(false && "Unimplemented");
    }
    virtual void visit(MemberExpr *expr) {
        assert(false && "Unimplemented");
    }
    virtual void visit(InfixExpr *expr) {
        auto lhs = genExpr(st, *expr->lhs)->asValue();
        auto rhs = genExpr(st, *expr->rhs)->asValue();

        llvm::Value *value;

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = st.builder.CreateAdd(lhs->llValue(), rhs->llValue(), "addResult");
        } break;
        case OPERATION_MINUS: {
            value = st.builder.CreateSub(lhs->llValue(), rhs->llValue(), "subResult");
        } break;
        case OPERATION_TIMES: {
            value = st.builder.CreateMul(lhs->llValue(), rhs->llValue(), "mulResult");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st.builder.CreateUDiv(lhs->llValue(), rhs->llValue(), "divResult");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = st.builder.CreateURem(lhs->llValue(), rhs->llValue(), "modResult");
        } break;
        }
    }
    virtual void visit(IfExpr *expr) {
        thing = genIf(st, expr->branches.data(), expr->branches.size());
    }
};



class StmtGen : public StmtVisitor {
    FnGenState &st;
public:
    StmtGen(FnGenState &st) : st(st) {}

    ValueThing *thing = NULL;

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = st.builder.CreateAlloca(st.prgm.getType(st.scope, stmt->type)->llType(),
                                               nullptr, stmt->name.data);

        // TODO: Allow undefined variables
        auto expr = genExpr(st, *stmt->value);
        std::cout << "HERE" << "\n";
        st.builder.CreateStore(expr->llValue(), alloca);
        std::cout << "THERE" << "\n";

        st.scope->addThing(stmt->name,
                           st.prgm.thing<STVThing>(alloca, expr->typeOf()));

        thing = NULL;
    }
    virtual void visit(ExprStmt *stmt) {
        thing = genExpr(st, *stmt->expr);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            st.builder.CreateRet(genExpr(st, *stmt->value)->llValue());
        } else {
            st.builder.CreateRetVoid();
        }

        thing = NULL;
    }
    virtual void visit(EmptyStmt *) {
        thing = NULL;
    }
};

ValueThing *genIf(FnGenState &st, Branch *branches, size_t count) {
    if (count > 0) {
        if (branches[0].cond != NULL) {
            auto cons = llvm::BasicBlock::Create(st.prgm.context, "ifCons", st.fn);
            auto alt = llvm::BasicBlock::Create(st.prgm.context, "ifAlt", st.fn);
            auto after = llvm::BasicBlock::Create(st.prgm.context, "afterIf", st.fn);

            auto cond = genExpr(st, *branches[0].cond);

            st.builder.CreateCondBr(cond->llValue(), cons, alt);

            // Generate the body
            st.builder.SetInsertPoint(cons);
            ValueThing *consVal = NULL;
            for (auto &stmt : branches[0].body) {
                consVal = genStmt(st, *stmt);
            }
            st.builder.CreateBr(after);

            // Generate the else expression
            st.builder.SetInsertPoint(alt);
            ValueThing *altVal = genIf(st, branches + 1, count - 1); // TODO: Eww, pointer math
            st.builder.CreateBr(after);

            // Generate the after block
            st.builder.SetInsertPoint(after);
            if (consVal != NULL && altVal != NULL) {
                assert(consVal->typeOf() == altVal->typeOf() && "Cons val and Alt val need the same type");
                auto phiNode = st.builder.CreatePHI(consVal->typeOf()->llType(), 2, "ifValue");
                phiNode->addIncoming(consVal->llValue(), cons);
                phiNode->addIncoming(altVal->llValue(), alt);

                return st.prgm.thing<STVThing>(phiNode,
                                               consVal->typeOf());
            } else {
                assert(consVal == altVal); // == NULL

                return NULL;
            }

        } else {
            // The else branch. It is unconditional
            ValueThing *consVal = NULL;
            for (auto &stmt : branches[0].body) {
                consVal = genStmt(st, *stmt);
            }
            return consVal;
        }
    } else {
        return NULL;
    }
}

inline ValueThing *genExpr(FnGenState &st, Expr &expr) {
    ExprGen eg(st);
    expr.accept(eg);

    // if (eg.thing == NULL)
    //     std::cerr << "** NULL: " << expr << "\n";
    return eg.thing;
}

inline ValueThing *genStmt(FnGenState &st, Stmt &stmt) {
    StmtGen sg(st);
    stmt.accept(sg);
    return sg.thing;
}
