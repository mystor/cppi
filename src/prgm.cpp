#include "prgm.h"
#include "gen.h"

#include <llvm/IR/Verifier.h>

// Add an item (from the AST) to the Program. This doesn't build the item, it just registers it
void Program::add_item(Item &item) {
    struct AddItemVisitor : public ItemVisitor {
        AddItemVisitor(Program *prgm) : prgm(prgm) {};
        Program *prgm;

        virtual void visit(FunctionItem *item) {
            Function fn = {
                .proto = &item->proto,
                .body = &item->body,
                .ir_repr = NULL
            };

            assert(!prgm->code_units.count(ProgIdent(item->proto.name)));

            prgm->code_units[ProgIdent(item->proto.name)] = { .data = fn };
        };

        virtual void visit(StructItem *item) {
            Struct st = {
                .name = item->name,
                .attributes = &item->args,
                .ir_repr = NULL
            };

            assert(!prgm->code_units.count(ProgIdent(item->name)));

            prgm->code_units[ProgIdent(item->name)] = { .data = st };
        };

        virtual void visit(FFIFunctionItem *item) {
            Function fn = {
                .proto = &item->proto,
                .body = NULL,
                .ir_repr = NULL
            };

            assert(!prgm->code_units.count(ProgIdent(item->proto.name)));

            prgm->code_units[ProgIdent(item->proto.name)] = { .data = fn };
        };

        virtual void visit(EmptyItem *) { /* pass */ };
    };

    AddItemVisitor visitor(this);
    item.accept(visitor);
}

void Program::add_items(std::vector<std::unique_ptr<Item>> &items) {
    for (auto &item : items) {
        add_item(*item);
    }
}

struct CodeUnitBuildVisitor : boost::static_visitor<void> {
    CodeUnitBuildVisitor(Program &prgm) : prgm(prgm) {};
    Program &prgm;

    void operator()(Placeholder) const {
        assert(false && "Cannot build a placeholder Code Unit");
    }

    void operator()(Function data) const {
        // Building the function prototype

        std::vector<llvm::Type *> arg_types;
        for (auto arg : data.proto->arguments) {
            arg_types.push_back(get_type(prgm, arg.type));
        }
        auto ft = llvm::FunctionType::get(get_type(prgm, data.proto->return_type),
                                          arg_types, false);

        auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, data.proto->name.data, prgm.module);

        if (fn->getName() != data.proto->name.data) {
            // This declaration has already been made
            fn->eraseFromParent();
            fn = prgm.module->getFunction(data.proto->name.data);

            // If we have already defined a body, then that's a problem!
            if (! fn->empty()) {
                std::cerr << "Error: Redefinition of function " << data.proto->name << ".";
                assert(false && "Function Redefinition");
            }

            // Check if the argument list is the same
            // TODO: Implement
        }

        // Insert it into the global scope!
        prgm.scope.vars.emplace(data.proto->name, fn);

        Scope old_scope = prgm.scope;

        // Set argument names
        unsigned idx = 0;
        for (auto ai = fn->arg_begin(); idx != data.proto->arguments.size(); ++ai, ++idx) {
            auto name = data.proto->arguments[idx].name;
            ai->setName(name.data);

            if (data.body != NULL) {
                prgm.scope.vars.emplace(name, ai);
            }
        }

        if (data.body != NULL) {
            // Creating the function's body!
            auto bb = llvm::BasicBlock::Create(prgm.context, "entry", fn);
            prgm.builder.SetInsertPoint(bb);
            prgm.current_function = fn;

            // Generate the body!
            for (auto &stmt : *data.body) {
                gen_stmt(prgm, *stmt);
            }

            // Revert to the previous scope state.
            prgm.scope = old_scope;
            prgm.current_function = NULL; // TODO(michael): If we ever build nested functions like this, we shouldn't do this

            // TODO: Implicit return statements!
            llvm::verifyFunction(*fn);
        }
    }

    void operator()(Struct data) const {

        assert(false && "Not implemented yet");
    }
};

void Program::build(CodeUnit *codeunit) {
    assert(codeunit);
    boost::apply_visitor(CodeUnitBuildVisitor(*this), codeunit->data);
}

void Program::build_all() {
    for (auto &bucket : code_units) {
        build(&bucket.second);
    }
}
