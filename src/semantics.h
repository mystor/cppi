//
//  semantics.h
//  cppl
//
//  Created by Michael Layzell on 2015-02-24.
//  Copyright (c) 2015 Michael Layzell. All rights reserved.
//

#ifndef __cppl__semantics__
#define __cppl__semantics__

#include "ast.h"
#include "intern.h"

#include <vector>
#include <unordered_map>
#include <queue>








// The phases of the semantic phase
//

struct Function {
    uint64_t uid;


};

struct Scope {
    std::unordered_map<istr, uint64_t> name_to_uid;
};


struct SemState {
    uint64_t currUid = 0;
    // This map keeps track of the identifier name for
    // every unique identifier in the program - for error
    // message reporting purposes
    std::unordered_map<uint64_t, istr> uid_to_name;
    std::unordered_map<uint64_t, llvm::Value *> uid_to_value;

    std::vector<Scope> scopes;

    std::queue<Function> functions;
};


void analyze(SemState *s, std::vector<std::unique_ptr<Item>> *i) {
    // Discover the global scope
    struct ItemGenState : ItemVisitor {
        SemState *s;
        ItemGenState(SemState *s) : s(s) {};

        void visit(FunctionItem *item) {
            Scope &lastScope = s->scopes.back();

            lastScope
        };
        void visit(StructItem *item) {
        };
        void visit(FFIFunctionItem *item) {
        };
        void visit(EmptyItem *item) {
        };
    };

    for (auto &item : *i) {
        ItemGenState igs(s);
        item->accept(igs);
    }

}


#endif
