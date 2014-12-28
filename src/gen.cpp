#include "gen.h"
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

using namespace llvm;

struct ParserState {
    LLVMContext &context;
    Module *module;
    IRBuilder<> &builder;
};

class ItemGen : public ItemVisitor {
    ParserState *st;
public:
    ItemGen(ParserState *st) : st(st) {}

    virtual void visit(FunctionItem *item) {
        // Create a function!
    }

    virtual void visit(EmptyItem *) {/* no-op */}
};

class StmtGen : public StmtVisitor {
    ParserState *st;
public:
    StmtGen(ParserState *st) : st(st) {}

    virtual void visit(DeclarationStmt *stmt) {

    }
    virtual void visit(ExprStmt *stmt) {
    }
    virtual void visit(EmptyStmt *) {/* no-op */}
};

class ExprGen : public ExprVisitor {
    ParserState *st;
public:
    ExprGen(ParserState *st) : st(st) {}

    virtual void visit(StringExpr *expr) {
    }
    virtual void visit(IntExpr *expr) {
    }
    virtual void visit(IdentExpr *expr) {
    }
    virtual void visit(CallExpr *expr) {
    }
    virtual void visit(InfixExpr *expr) {
    }
};

Module *generate_module(std::vector<std::unique_ptr<Item>> items) {
    LLVMContext &context = getGlobalContext();
    Module *module = new Module("MyModuleName", context);
    IRBuilder<> builder = IRBuilder<>(context);

    ParserState st { context, module, builder };
    for (auto &item : items) {
        ItemGen gen(&st);
        item->accept(gen);
    }

    return module;
}
