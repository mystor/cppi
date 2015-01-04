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

struct Scope {
    std::unordered_map<istr, llvm::Value*> vars;
    std::unordered_map<istr, llvm::Type*> types;
};

// A program consists of a set of code units. It's produced from those code units mostly
struct Program {
    Scope scope = {};
    llvm::Function * current_function = NULL;

    llvm::LLVMContext &context;
    llvm::Module *module;
    llvm::IRBuilder<> builder;

    Program(llvm::LLVMContext &context = llvm::getGlobalContext())
        : context(context),
          module(new llvm::Module("cppi_module", context)),
          builder(llvm::IRBuilder<>(context)) {}

    std::unordered_map<ProgIdent, CodeUnit> code_units;

    void add_item(Item &item);
    void add_items(std::vector<std::unique_ptr<Item>> &items);

    void build(CodeUnit *codeunit);
    void build_all();
};

#endif /* defined(__cppl__prgm__) */
