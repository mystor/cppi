//
//  ast.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__ast__
#define __cppl__ast__

#include <memory>
#include <vector>
#include "lexer.h"

class Type {
public:
	Type() { ident = intern("void"); };
	Type(const char *ident) : ident(ident) {};
	const char *ident;
};

const Type TYPE_NULL = Type(intern("void"));

class Argument {
public:
	Argument(const char *name, Type type) : name(name), type(type) {};
	const char *name;
	Type type;
};

// This will represent an arbitrary ast node
// Very exciting, I know
class Expr {
};

class StringExpr : public Expr {
public:
	StringExpr(const char *value) : value(value) {};
	const char *value; // Interned!
};

class CallExpr : public Expr {
public:
	CallExpr(Expr callee, std::vector<Expr> args) : callee(callee), args(args) {};
	Expr callee;
	std::vector<Expr> args;
};

class IdentExpr : public Expr {
public:
	IdentExpr(const char *ident) : ident(ident) {};
	const char *ident;
};

enum OperationType {
	OPERATION_PLUS,
	OPERATION_MINUS,
	OPERATION_TIMES,
	OPERATION_DIVIDE,
	OPERATION_MODULO
};

class InfixExpr : public Expr {
public:
	InfixExpr(OperationType op, Expr lhs, Expr rhs) : op(op), lhs(lhs), rhs(rhs) {};
	OperationType op;
	Expr lhs;
	Expr rhs;
};

class Stmt {
};

class DeclarationStmt : public Stmt {
public:
	const char *name;
	Expr value;
};

class FunctionStmt : public Stmt {
public:
	const char *name;
	std::vector<Argument> arguments;
	Type return_type;
	std::vector<Stmt> body;
};

class ExprStmt : public Stmt {
public:
	ExprStmt(Expr expr) : expr(expr) {};
	Expr expr;
};

class EmptyStmt : public Stmt {
};
const EmptyStmt EMPTY_STMT = EmptyStmt();


// Yeah, we're throwing the parser stuff in here too, 'cause why not!
// Yes, its the wrong place to do it.
std::vector<Stmt> parse(Lexer *lex);
Stmt parse_stmt(Lexer *lex);
Expr parse_expr(Lexer *lex);

#endif /* defined(__cppl__ast__) */
