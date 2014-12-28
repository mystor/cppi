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

class StmtGen : public StmtVisitor {
    ParserState *st;
public:
    StmtGen(ParserState *st) : st(st) {}

    virtual void visit(DeclarationStmt *stmt) {

    }
    virtual void visit(FunctionStmt *stmt) {
    }
    virtual void visit(ExprStmt *stmt) {
    }
    virtual void visit(EmptyStmt *stmt) {
    }
};

Module *generate_module(std::vector<std::unique_ptr<Stmt>> stmts) {
    LLVMContext &context = getGlobalContext();
    Module *module = new Module("MyModuleName", context);
    IRBuilder<> builder = IRBuilder<>(context);

    ParserState st { context, module, builder };
    for (auto &stmt : stmts) {
        StmtGen gen(&st);

        stmt->accept(gen);
    }

    return module;
}
