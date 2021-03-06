//
//  gen.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__gen__
#define __cppl__gen__

#include "ast.h"
#include "prgm.h"

#include <vector>
#include <memory>
#include <unordered_map>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

// llvm::Type *get_type(Scope *scope, Type &ty);
ValueThing *genExpr(Program &prgm, Expr &expr);
ValueThing *genStmt(Program &prgm, Stmt &stmt);

#endif /* defined(__cppl__gen__) */
