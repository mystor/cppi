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

    explicit Scope(Scope *parent) : parent(parent) {}

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

    void addThing(istr name, Thing *thing) {
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
    uint32_t pointerWidth = 8 * sizeof(void *); // TODO(michael): Make this actually represent the pointer width of target platform

    Builtin builtin;
    Scope *globalScope;

    llvm::LLVMContext &context;
    llvm::Module *module;

    // The current state of the code generator
    Scope *scope = NULL;
    llvm::Function *fn = NULL;
    llvm::IRBuilder<> builder;

    // The program is responsible for maintaining the lifetimes of scopes and things.
    // The vectors below hold unique_ptrs which will free the memory for the scopes
    // and things which have been allocated when the Program is freed.
    std::vector<std::unique_ptr<Thing>> things;
    std::vector<std::unique_ptr<Scope>> scopes;

    Program(llvm::LLVMContext &context = llvm::getGlobalContext())
        : context(context),
          module(new llvm::Module("cppi_module", context)),
          builder(llvm::IRBuilder<>(context)) {


        // Create the builtin objects and types
        builtin.init(*this);

        globalScope = mkScope(NULL);
        // Expose the builtin things to the cppl program
#define exposeBuiltin(name) globalScope->addThing(intern(#name), builtin.name)
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

    Scope *mkScope(Scope *parent) {
        auto u = std::make_unique<Scope>(parent);
        Scope *p = &*u;
        scopes.push_back(std::move(u));
        return p;
    }

    TypeThing *getType(Scope *scope, Type &ty) {
        auto thing = scope->thing(ty.ident);
        assert(thing != NULL);

        return thing->asType();
    }

    void addItem(Item &item);
    void addItems(std::vector<std::unique_ptr<Item>> &items);

    void finalize();
};

#endif /* defined(__cppl__prgm__) */
