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
ValueThing *genIf(Program &prgm, Branch *branches, size_t count);

class ExprGen : public ExprVisitor {
    Program &prgm;
public:
    ValueThing *thing = NULL;

    ExprGen(Program &prgm) : prgm(prgm) {}

    virtual void visit(StringExpr *expr) {
        // Create the string literal as a global string pointer
        // This includes the `\0` at the end of the string literal, which
        // is OK for us, because we want our code to support sending data
        // (like strings) to c programs easily

        auto data = prgm.builder.CreateGlobalStringPtr(expr->value.data, "stringLiteral");
        auto length = llvm::ConstantInt::get(llvm::IntegerType::get(prgm.context, prgm.pointerWidth), expr->value.length);

        // TODO(michael): This should internally probably use the generic slice type,
        // once we get to that point in terms of compiler construction
        llvm::StructType *stringTy = llvm::cast<llvm::StructType>(prgm.builtin.string->asType()->llType());

        // Allocate the string struct, and fill it with data - This allocal _should_ be optimized
        // out by the compiler at some point
        auto stringStruct = prgm.builder.CreateAlloca(stringTy);
        auto stringDataPtr = prgm.builder.CreateConstInBoundsGEP2_32(stringStruct, 0, 0);
        auto stringLenPtr = prgm.builder.CreateConstInBoundsGEP2_32(stringStruct, 0, 1);

        prgm.builder.CreateStore(data, stringDataPtr);
        prgm.builder.CreateStore(length, stringLenPtr);

        // Get it back into value-form
        thing = prgm.thing<STVThing>(prgm.builder.CreateLoad(stringStruct),
                                        prgm.builtin.string->asType())->asValue();
    }
    virtual void visit(IntExpr *expr) {
        // TODO: Not all ints are 32 bits
        thing = prgm.thing<STVThing>(llvm::ConstantInt::get(llvm::IntegerType::get(prgm.context, 32), expr->value),
                                        prgm.builtin.i32->asType())->asValue();
    }
    virtual void visit(BoolExpr *expr) {
        if (expr->value) {
            thing = prgm.thing<STVThing>(llvm::ConstantInt::getTrue(prgm.context),
                                            prgm.builtin.boolean->asType())->asValue();
        } else {
            thing = prgm.thing<STVThing>(llvm::ConstantInt::getFalse(prgm.context),
                                            prgm.builtin.boolean->asType())->asValue();
        }
    }
    virtual void visit(MkExpr *expr) {

        assert(false && "Unimplemented");
    }
    virtual void visit(IdentExpr *expr) {
        Thing *aThing = prgm.scope->thing(expr->ident);

        assert(aThing != NULL);

        thing = aThing->asValue();
        assert(aThing->asValue());
    }
    virtual void visit(CallExpr *expr) {
        auto callee = genExpr(prgm, *expr->callee)->asValue()->llValue();

        // TODO(michael): assert(callee->isCallable());

        std::vector<llvm::Value *> args;
        for (auto &arg : expr->args) {
            auto earg = genExpr(prgm, *arg)->asValue();
            assert(earg != NULL);

            // TODO(michael): Check types match that of function call

            args.push_back(earg->llValue());
        }

        thing = prgm.thing<STVThing>(prgm.builder.CreateCall(callee, args, "callResult"),
                                        (TypeThing *)NULL /* TODO IMPLEMENT */);
    }
    virtual void visit(MthdCallExpr *expr) {
        assert(false && "Unimplemented");
    }
    virtual void visit(MemberExpr *expr) {
        assert(false && "Unimplemented");
    }
    virtual void visit(InfixExpr *expr) {
        auto lhs = genExpr(prgm, *expr->lhs)->asValue();
        auto rhs = genExpr(prgm, *expr->rhs)->asValue();

        llvm::Value *value;

        switch (expr->op) {
        case OPERATION_PLUS: {
            value = prgm.builder.CreateAdd(lhs->llValue(), rhs->llValue(), "addResult");
        } break;
        case OPERATION_MINUS: {
            value = prgm.builder.CreateSub(lhs->llValue(), rhs->llValue(), "subResult");
        } break;
        case OPERATION_TIMES: {
            value = prgm.builder.CreateMul(lhs->llValue(), rhs->llValue(), "mulResult");
        } break;
        case OPERATION_DIVIDE: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = prgm.builder.CreateUDiv(lhs->llValue(), rhs->llValue(), "divResult");
        } break;
        case OPERATION_MODULO: {
            // TODO: Signed vs Unsigned. Right now only unsigned values
            value = prgm.builder.CreateURem(lhs->llValue(), rhs->llValue(), "modResult");
        } break;
        }
    }
    virtual void visit(IfExpr *expr) {
        thing = genIf(prgm, expr->branches.data(), expr->branches.size());
    }
};



class StmtGen : public StmtVisitor {
    Program &prgm;
public:
    StmtGen(Program &prgm) : prgm(prgm) {}

    ValueThing *thing = NULL;

    virtual void visit(DeclarationStmt *stmt) {
        auto alloca = prgm.builder.CreateAlloca(prgm.getType(prgm.scope, stmt->type)->llType(),
                                               nullptr, stmt->name.data);

        // TODO: Allow undefined variables
        auto expr = genExpr(prgm, *stmt->value);
        prgm.builder.CreateStore(expr->llValue(), alloca);

        prgm.scope->addThing(stmt->name,
                           prgm.thing<STVThing>(alloca, expr->typeOf()));

        thing = NULL;
    }
    virtual void visit(ExprStmt *stmt) {
        thing = genExpr(prgm, *stmt->expr);
    }
    virtual void visit(ReturnStmt *stmt) {
        if (stmt->value != nullptr) {
            prgm.builder.CreateRet(genExpr(prgm, *stmt->value)->llValue());
        } else {
            prgm.builder.CreateRetVoid();
        }

        thing = NULL;
    }
    virtual void visit(EmptyStmt *) {
        thing = NULL;
    }
};

ValueThing *genIf(Program &prgm, Branch *branches, size_t count) {
    if (count > 0) {
        if (branches[0].cond != NULL) {
            auto cons = llvm::BasicBlock::Create(prgm.context, "ifCons", prgm.fn);
            auto alt = llvm::BasicBlock::Create(prgm.context, "ifAlt", prgm.fn);
            auto after = llvm::BasicBlock::Create(prgm.context, "afterIf", prgm.fn);

            auto cond = genExpr(prgm, *branches[0].cond);

            prgm.builder.CreateCondBr(cond->llValue(), cons, alt);

            // Generate the body
            prgm.builder.SetInsertPoint(cons);
            ValueThing *consVal = NULL;
            for (auto &stmt : branches[0].body) {
                consVal = genStmt(prgm, *stmt);
            }
            prgm.builder.CreateBr(after);

            // Generate the else expression
            prgm.builder.SetInsertPoint(alt);
            ValueThing *altVal = genIf(prgm, branches + 1, count - 1); // TODO: Eww, pointer math
            prgm.builder.CreateBr(after);

            // Generate the after block
            prgm.builder.SetInsertPoint(after);
            if (consVal != NULL && altVal != NULL) {
                assert(consVal->typeOf() == altVal->typeOf() && "Cons val and Alt val need the same type");
                auto phiNode = prgm.builder.CreatePHI(consVal->typeOf()->llType(), 2, "ifValue");
                phiNode->addIncoming(consVal->llValue(), cons);
                phiNode->addIncoming(altVal->llValue(), alt);

                return prgm.thing<STVThing>(phiNode,
                                               consVal->typeOf());
            } else {
                assert(consVal == altVal); // == NULL

                return NULL;
            }

        } else {
            // The else branch. It is unconditional
            ValueThing *consVal = NULL;
            for (auto &stmt : branches[0].body) {
                consVal = genStmt(prgm, *stmt);
            }
            return consVal;
        }
    } else {
        return NULL;
    }
}

inline ValueThing *genExpr(Program &prgm, Expr &expr) {
    ExprGen eg(prgm);
    expr.accept(eg);

    // if (eg.thing == NULL)
    //     std::cerr << "** NULL: " << expr << "\n";
    return eg.thing;
}

inline ValueThing *genStmt(Program &prgm, Stmt &stmt) {
    StmtGen sg(prgm);
    stmt.accept(sg);
    return sg.thing;
}
