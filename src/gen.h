//
//  gen.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__gen__
#define __cppl__gen__

#include <vector>
#include <memory>
#include <llvm/IR/Module.h>
#include "ast.h"

llvm::Module *generate_module(std::vector<std::unique_ptr<Item>> items);

#endif /* defined(__cppl__gen__) */
