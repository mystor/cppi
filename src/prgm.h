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
#include <llvm/IR/Function.h>

struct ProgIdent {
    ProgIdent(const char* name) { path.push_back(name); };
    std::vector<const char*> path;

    bool operator==(const ProgIdent &other) {
        return path == other.path;
    }
};

// TODO: This is currently a super-ghetto hash function
namespace std {
    template <>
    struct hash<ProgIdent> {
        std::size_t operator()(const ProgIdent &p) const {
            using std::size_t;
            using std::hash;
            using std::string;

            // Yeahhh. I should do this better....
            size_t hsh = 0;
            for (auto segment : p.path) {
                hsh ^= hash<const char *>()(segment);
            }
            return hsh;
        }
    };
}

// Code in cppl is compiled in code units. Code units include things like functions, and
// Structures. Every code unit can have dependencies on another code unit before it can be executed,
// and dependencies on another code unit before it can be compiled. These are seperate
struct CodeUnit {
    std::unordered_set<ProgIdent> rt_deps; // Runtime depenedncies (may be cyclic)
    std::unordered_set<ProgIdent> ct_deps; // Compile-time dependencies (may not be cyclic)

    virtual void abstract_as_shit() = 0;
};

enum FunctionVariety {
    FN_VAR_AST,
    FN_VAR_IR,
    FN_VAR_FFI
};

// This is a function in the language It is a toplevel structure, and has dependencies on other toplevel structures etc
struct Function : public CodeUnit {
    FunctionVariety variety;

    // May be NULL
    FunctionItem *ast_node;

    FunctionProto *proto;

    // May be NULL
    llvm::Function *ir_repr;
};

enum StructVariety {
    STRUCT_VAR_AST,
    STRUCT_VAR_IR
};

// A structure in the language, wow, so amaze
struct Struct : public CodeUnit {
    StructVariety variety;



    llvm::StructType *ir_repr;
};

struct Placeholder : public CodeUnit {};

// A program consists of a set of code units. It's produced from those code units mostly
struct Program {
    std::unordered_map<ProgIdent, CodeUnit> code_units;

    CodeUnit *get_item_by_name(ProgIdent ident);

    // Add a single item to the program
    void add_item(Item &item);

    // Process a set of items and add the corresponding code units to the Program
    void add_items(std::vector<std::unique_ptr<Item>> &items);
};

#endif /* defined(__cppl__prgm__) */
