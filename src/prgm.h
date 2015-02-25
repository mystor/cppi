//
//  prgm.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-30.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__prgm__
#define __cppl__prgm__

#include "ast.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <boost/variant.hpp>

/*
// This is some nasty mess to enable the hashing of an object consisting of
// multiple sections. Each of those sections consists of an interned string

struct ProgIdent {
    ProgIdent(istr name) { path.push_back(name); };
    std::vector<istr> path;

    std::string as_string() {
        std::string str;
        bool first = true;
        for (auto seg : path) {
            if (first) { first = false; } else { str += "::"; }
            str += seg.data;
        }
        return str;
    }

    bool operator==(const ProgIdent &other) const {
        return path == other.path;
    }
};

// TODO: This is currently a super-ghetto hash function
namespace std {
    template <>
    struct hash<ProgIdent> {
        std::size_t operator()(const ProgIdent &p) const {
            // Yeahhh. I should do this better....
            size_t hsh = 0;
            for (auto segment : p.path) {
                hsh ^= hash<istr>()(segment);
            }
            return hsh;
        }
    };
}

*/

/*
struct Placeholder {};

// This is a function in the language It is a toplevel structure, and has dependencies on other toplevel structures etc
struct Function {
    FunctionProto *proto;

    // May be NULL
    std::vector<std::unique_ptr<Stmt>> *body;

    // May be NULL
    llvm::Function *ir_repr;
};

// A structure in the language, wow, so amaze
struct Struct {
    istr name;

    // TODO(michael): Attributes ought to not have the argument type...
    std::vector<Argument> *attributes;

    llvm::StructType *ir_repr;
};

struct CodeUnit {
    bool built = false;
    boost::variant<Placeholder, Function, Struct> data;
};
*/

struct TypeThing;
struct ValueThing;

struct Thing {
    virtual ValueThing *asValue() { return NULL; }
    virtual TypeThing *asType() { return NULL; }

    virtual void finalize() {};
    virtual void print(llvm::raw_ostream &os) = 0;
};

struct TypeThing : public virtual Thing {
    virtual llvm::Type *llType() = 0;
};

struct ValueThing : public virtual Thing {
    virtual llvm::Value *llValue() = 0;
    virtual TypeThing *typeOf() = 0;
};

struct Scope {
    Scope *parent;
    std::unordered_map<istr, Thing *> things;

    Thing *thing(istr name) {
        assert(this != parent);

        auto found = things.find(name);
        if (found == things.end()) {
            return parent != NULL ? parent->thing(name) : NULL;
        } else {
            assert(found->second != NULL);
            return found->second;
        }
    }

    void push_thing(istr name, Thing *thing) {
        assert(things.find(name) == things.end());

        things.emplace(name, thing);
    }
};

struct Program;
struct Builtin {
    Builtin() {};

    void init(Program &p);

    Thing *string;
    Thing *i8;
    Thing *i16;
    Thing *i32;
    Thing *i64;
    Thing *f16;
    Thing *f32;
    Thing *f64;
    Thing *boolean;
};

// A program consists of a set of code units. It's produced from those code units mostly
struct Program {
    uint32_t pointer_width = 8 * sizeof(void *); // TODO(michael): Make this actually represent the pointer width of target platform

    Builtin builtin;
    Scope global_scope;

    llvm::LLVMContext &context;
    llvm::Module *module;

    // A place for storing things
    std::vector<std::unique_ptr<Thing>> things;

    Program(llvm::LLVMContext &context = llvm::getGlobalContext())
        : context(context),
          module(new llvm::Module("cppi_module", context)) {


        // Create the builtin objects and types
        builtin.init(*this);

        global_scope = { NULL, std::unordered_map<istr, Thing*>() };

        // Expose the builtin things to the cppl program
#define exposeBuiltin(name) global_scope.push_thing(intern(#name), builtin.name)
        exposeBuiltin(i8);
        exposeBuiltin(i16);
        exposeBuiltin(i32);
        exposeBuiltin(i64);

        exposeBuiltin(f16);
        exposeBuiltin(f32);
        exposeBuiltin(f64);

        exposeBuiltin(string);
        exposeBuiltin(boolean);
#undef exposeBuiltin
    }

    // A templated function for creating classes
    template< class T, class... Args >
    T *thing( Args&&... args ) {
        std::unique_ptr<T> u = std::make_unique<T>( *this, args... );
        T *p = &*u;
        things.push_back(std::move(u));
        return p;
    }

    TypeThing *GetType(Scope *scope, Type &ty) {
        auto thing = scope->thing(ty.ident);
        assert(thing != NULL);

        return thing->asType();
    }

    void add_item(Item &item);
    void add_items(std::vector<std::unique_ptr<Item>> &items);

    void finalize();
};

#endif /* defined(__cppl__prgm__) */
