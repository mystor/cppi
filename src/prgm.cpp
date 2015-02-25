#include "prgm.h"
#include "gen.h"

#include <llvm/IR/Verifier.h>

// TODO(michael): When namespaces become a thing, these primitives should have
// a type which
struct PrimTypeThing : public TypeThing {
    llvm::Type *typeImpl;

    PrimTypeThing(Program &, llvm::Type *typeImpl) : typeImpl(typeImpl) {};

    TypeThing *asType() { return this; }

    llvm::Type *llType() {
        return typeImpl;
    }

    void print(llvm::raw_ostream &os) {
        os << "PrimTypeThing(" << *typeImpl << ")";
    }
};

// Initialize the builtin object with a bunch or primitive types
void Builtin::init(Program &p) {
    i8 = p.thing<PrimTypeThing>(llvm::Type::getInt8Ty(p.context));
    i16 = p.thing<PrimTypeThing>(llvm::Type::getInt16Ty(p.context));
    i32 = p.thing<PrimTypeThing>(llvm::Type::getInt32Ty(p.context));
    i64 = p.thing<PrimTypeThing>(llvm::Type::getInt64Ty(p.context));

    f16 = p.thing<PrimTypeThing>(llvm::Type::getHalfTy(p.context));
    f32 = p.thing<PrimTypeThing>(llvm::Type::getFloatTy(p.context));
    f64 = p.thing<PrimTypeThing>(llvm::Type::getDoubleTy(p.context));

    boolean = p.thing<PrimTypeThing>(llvm::Type::getInt1Ty(p.context));

    std::vector<llvm::Type *> string_attrs = { llvm::Type::getInt8PtrTy(p.context), llvm::Type::getIntNTy(p.context, p.pointer_width) };
    auto string_ty = llvm::StructType::create(p.context, string_attrs, "string");

    string = p.thing<PrimTypeThing>(string_ty);
}


llvm::Function *llFromProto(Program &prgm, FunctionProto *proto) {
    std::cout << "** Building function " << proto->name << "\n";
    // Building the function prototype

    std::vector<llvm::Type *> arg_types;
    for (auto arg : proto->arguments) {
        arg_types.push_back(prgm.GetType(&prgm.global_scope, arg.type)->llType());
    }
    auto ft = llvm::FunctionType::get(prgm.GetType(&prgm.global_scope, proto->return_type)->llType(),
                                      arg_types, false);

    auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, proto->name.data, prgm.module);

    if (fn->getName() != proto->name.data) {
        assert(false && "Function Redefinition");

        /*
        // This declaration has already been made
        fn->eraseFromParent();
        fn = prgm.module->getFunction(proto->name.data);

        // If we have already defined a body, then that's a problem!
        if (! fn->empty()) {
            std::cerr << "Error: Redefinition of function " << proto->name << ".";
            assert(false && "Function Redefinition");
        }

        // Check if the argument list is the same
        // TODO: Implement
        */
    }

    // Set argument names
    unsigned idx = 0;
    for (auto ai = fn->arg_begin(); idx != proto->arguments.size(); ++ai, ++idx) {
        auto name = proto->arguments[idx].name;
        ai->setName(name.data);
    }

    return fn;
}

struct FunctionThing : ValueThing {
    Program &prgm;
    FunctionProto *proto;
    std::vector<std::unique_ptr<Stmt>> *body;

    llvm::Function *fn = NULL;

    FunctionThing(Program &prgm, FunctionProto *proto, std::vector<std::unique_ptr<Stmt>> *body) : prgm(prgm), proto(proto), body(body) {};

    ValueThing *asValue() { return this; }

    llvm::Value *llValue() {
        if (fn == NULL) {
            fn = llFromProto(prgm, proto);
        }

        return fn;
    }

    void finalize() {
        llValue(); // Ensure that fn is set

        FnGenState gs(prgm);
        gs.fn = fn;

        // TODO(michael): Fix up the scope

        auto bb = llvm::BasicBlock::Create(prgm.context, "entry", fn);
        gs.builder.SetInsertPoint(bb);

        for (auto &stmt : *body) {
            gen_stmt(gs, *stmt);
        }

        llvm::verifyFunction(*fn);
    }

    TypeThing *typeOf() {
        assert(false && "Unimplemented!");
    }

    void print(llvm::raw_ostream &os) {
        os << "FunctionThing";
    }
};

struct FFIFunctionThing : ValueThing {
    Program &prgm;
    FunctionProto *proto;

    llvm::Function *fn = NULL;

    FFIFunctionThing(Program &prgm, FunctionProto *proto) : prgm(prgm), proto(proto) {};

    ValueThing *asValue() { return this; }

    llvm::Value *llValue() {
        if (fn == NULL) {
            fn = llFromProto(prgm, proto);
        }

        return fn;
    }

    TypeThing *typeOf() {
        assert(false && "Unimplemented!");
    };

    void print(llvm::raw_ostream &os) {
        os << "FFIFunctionThing";
    }
};

struct StructDefThing : TypeThing {
    Program &prgm;
    std::vector<Argument> *attrs;
    llvm::StructType *typeImpl = NULL;

    StructDefThing(Program &prgm, std::vector<Argument> *attrs) : prgm(prgm), attrs(attrs) {};

    TypeThing *asType() { return this; }

    llvm::Type *llType() {
        if (typeImpl != NULL) return typeImpl;


        assert(false && "Unimplemented!");
    }

    void print(llvm::raw_ostream &os) {
        os << "StructDefThing";
    }
};

// Add an item (from the AST) to the Program. This doesn't build the item, it just registers it
void Program::add_item(Item &item) {
    struct AddItemVisitor : public ItemVisitor {
        AddItemVisitor(Program &prgm) : prgm(prgm) {};
        Program &prgm;

        virtual void visit(FunctionItem *item) {
            auto fthing = prgm.thing<FunctionThing>(&item->proto, &item->body);

            prgm.global_scope.push_thing(item->proto.name, fthing);
        };

        virtual void visit(StructItem *item) {
            auto sdthing = prgm.thing<StructDefThing>(&item->args);

            prgm.global_scope.push_thing(item->name, sdthing);
        };

        virtual void visit(FFIFunctionItem *item) {
            auto fthing = prgm.thing<FFIFunctionThing>(&item->proto);

            prgm.global_scope.push_thing(item->proto.name, fthing);
        };

        virtual void visit(EmptyItem *) { /* pass */ };
    };

    AddItemVisitor visitor(*this);
    item.accept(visitor);
}

void Program::add_items(std::vector<std::unique_ptr<Item>> &items) {
    for (auto &item : items) {
        add_item(*item);
    }
}

void Program::finalize() {
    // Finalize all the things!
    // This is done like this rather than with an iterator because
    // the things vector may be appended to during the finalize method
    for (size_t i=0; i<things.size(); i++) {
        // std::cout << "finalizing: " << &*things[i] << "\n";
        things[i]->finalize();
    }
}

/*
struct CodeUnitBuildVisitor : boost::static_visitor<void> {
    CodeUnitBuildVisitor(Program &prgm) : prgm(prgm) {};
    Program &prgm;

    void operator()(Placeholder) const {
        assert(false && "Cannot build a placeholder Code Unit");
    }

    void operator()(Function data) const {
        std::cout << "** Building function " << data.proto->name << "\n";
        // Building the function prototype

        std::vector<llvm::Type *> arg_types;
        for (auto arg : data.proto->arguments) {
            arg_types.push_back(get_type(&prgm.global_scope, arg.type));
        }
        auto ft = llvm::FunctionType::get(get_type(&prgm.global_scope, data.proto->return_type),
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
        prgm.global_scope.vars.emplace(data.proto->name, fn);

        // TODO(michael): This is really sloppy, it would be nicer to have seperate states
        // for seperate compilation runs, which contain a ref back to the program...
        Scope scope = { &prgm.global_scope, std::unordered_map<istr, llvm::Value*>(), std::unordered_map<istr, llvm::Type*>() };
        FnGenState gen_state(prgm);
        gen_state.fn = fn;
        gen_state.scope = &scope;

        // Set argument names
        unsigned idx = 0;
        for (auto ai = fn->arg_begin(); idx != data.proto->arguments.size(); ++ai, ++idx) {
            auto name = data.proto->arguments[idx].name;
            ai->setName(name.data);

            if (data.body != NULL) {
                gen_state.scope->vars.emplace(name, ai);
            }
        }

        if (data.body != NULL) {
            // Creating the function's body!
            auto bb = llvm::BasicBlock::Create(prgm.context, "entry", fn);
            gen_state.builder.SetInsertPoint(bb);

            for (auto &stmt : *data.body) {
                gen_stmt(gen_state, *stmt);
            }

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
    if (! codeunit->built) {
        codeunit->built = true; // TODO(michael): Not _completely_ true yet. Maybe a building flag?
        boost::apply_visitor(CodeUnitBuildVisitor(*this), codeunit->data);
    }
}

void Program::build_all() {
    for (auto &bucket : code_units) {
        build(&bucket.second);
    }
}
*/
