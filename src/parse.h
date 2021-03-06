//
//  parse.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-28.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__parse__
#define __cppl__parse__

#include <vector>
#include <memory>
#include "ast.h"

// Parse an entire program
std::vector<std::unique_ptr<Item>> parse(Lexer *lex);

// Parse an individual item
std::unique_ptr<Item> parseItem(Lexer *lex);
// Parse an individual statement
std::unique_ptr<Stmt> parseStmt(Lexer *lex);
// Parse an expression
std::unique_ptr<Expr> parseExpr(Lexer *lex);

#endif /* defined(__cppl__parse__) */
