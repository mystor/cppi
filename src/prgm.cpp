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

    std::vector<llvm::Type *> stringAttrs = {
        llvm::Type::getInt8PtrTy(p.context),
        llvm::Type::getIntNTy(p.context, p.pointerWidth)
    };
    auto stringTy = llvm::StructType::create(p.context, stringAttrs, "string");

    string = p.thing<PrimTypeThing>(stringTy);
}


llvm::Function *llFromProto(Program &prgm, FunctionProto *proto) {
    std::cout << "** Building function " << proto->name << "\n";
    // Building the function prototype

    std::vector<llvm::Type *> arg_types;
    for (auto arg : proto->arguments) {
        arg_types.push_back(prgm.getType(prgm.globalScope, arg.type)->llType());
    }
    auto ft = llvm::FunctionType::get(prgm.getType(prgm.globalScope, proto->returnType)->llType(),
                                      arg_types, false);

    auto fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, proto->name.data, prgm.module);

    if (fn->getName() != proto->name.data) {
        assert(false && "Function Redefinition");
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

        FnGenState gs(prgm, prgm.scope(prgm.globalScope), fn);

        // TODO(michael): Fix up the scope

        auto bb = llvm::BasicBlock::Create(prgm.context, "entry", fn);
        gs.builder.SetInsertPoint(bb);

        for (auto &stmt : *body) {
            genStmt(gs, *stmt);
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
void Program::addItem(Item &item) {
    struct AddItemVisitor : public ItemVisitor {
        AddItemVisitor(Program &prgm) : prgm(prgm) {};
        Program &prgm;

        virtual void visit(FunctionItem *item) {
            auto fthing = prgm.thing<FunctionThing>(&item->proto, &item->body);

            prgm.globalScope->addThing(item->proto.name, fthing);
        };

        virtual void visit(StructItem *item) {
            auto sdthing = prgm.thing<StructDefThing>(&item->args);

            prgm.globalScope->addThing(item->name, sdthing);
        };

        virtual void visit(FFIFunctionItem *item) {
            auto fthing = prgm.thing<FFIFunctionThing>(&item->proto);

            prgm.globalScope->addThing(item->proto.name, fthing);
        };

        virtual void visit(EmptyItem *) { /* pass */ };
    };

    AddItemVisitor visitor(*this);
    item.accept(visitor);
}

void Program::addItems(std::vector<std::unique_ptr<Item>> &items) {
    for (auto &item : items) {
        addItem(*item);
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
