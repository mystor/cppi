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
std::ostream& operator<<(std::ostream& os, Type &type);

const Type TYPE_NULL = Type(intern("void"));

class Argument {
public:
	Argument(const char *name, Type type) : name(name), type(type) {};
	const char *name;
	Type type;
};
std::ostream& operator<<(std::ostream& os, Argument &arg);

// This will represent an arbitrary ast node
// Very exciting, I know
class Expr {
public:
	virtual std::ostream& show(std::ostream& os) = 0;
};

std::ostream& operator<<(std::ostream& os, Expr &expr);

class StringExpr : public Expr {
public:
	StringExpr(const char *value) : value(value) {};
	const char *value; // Interned!
	virtual std::ostream& show(std::ostream& os);
};

class CallExpr : public Expr {
public:
	CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args) : callee(std::move(callee)), args(std::move(args)) {};
	std::unique_ptr<Expr> callee;
	std::vector<std::unique_ptr<Expr>> args;
	virtual std::ostream& show(std::ostream& os);
};

class IdentExpr : public Expr {
public:
	IdentExpr(const char *ident) : ident(ident) {};
	const char *ident;
	virtual std::ostream& show(std::ostream& os);
};

enum OperationType {
	OPERATION_PLUS,
	OPERATION_MINUS,
	OPERATION_TIMES,
	OPERATION_DIVIDE,
	OPERATION_MODULO
};

std::ostream& operator<<(std::ostream& os, OperationType opty);

class InfixExpr : public Expr {
public:
	InfixExpr(OperationType op,
			  std::unique_ptr<Expr> lhs,
			  std::unique_ptr<Expr> rhs) : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {};
	OperationType op;
	std::unique_ptr<Expr> lhs;
	std::unique_ptr<Expr> rhs;
	virtual std::ostream& show(std::ostream& os);
};

class Stmt {
public:
	virtual std::ostream& show(std::ostream& os) = 0;
};

std::ostream& operator<<(std::ostream& os, Stmt &stmt);

class DeclarationStmt : public Stmt {
public:
	DeclarationStmt(const char *name, Type type, std::unique_ptr<Expr> value) : name(name), type(type), value(std::move(value)) {};
	const char *name;
	Type type;
	std::unique_ptr<Expr> value;
	virtual std::ostream& show(std::ostream& os);
};

class FunctionStmt : public Stmt {
public:
	FunctionStmt(const char *name, std::vector<Argument> arguments, Type return_type, std::vector<std::unique_ptr<Stmt>> body) : name(name), arguments(arguments), return_type(return_type), body(std::move(body)) {};
	const char *name;
	std::vector<Argument> arguments;
	Type return_type;
	std::vector<std::unique_ptr<Stmt>> body;
	virtual std::ostream& show(std::ostream& os);
};

class ExprStmt : public Stmt {
public:
	ExprStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {};
	std::unique_ptr<Expr> expr;
	virtual std::ostream& show(std::ostream& os);
};

class EmptyStmt : public Stmt {
public:
	virtual std::ostream& show(std::ostream& os);
};
const EmptyStmt EMPTY_STMT = EmptyStmt();


// Yeah, we're throwing the parser stuff in here too, 'cause why not!
// Yes, its the wrong place to do it.
std::vector<std::unique_ptr<Stmt>> parse(Lexer *lex);
std::unique_ptr<Stmt> parse_stmt(Lexer *lex);
std::unique_ptr<Expr> parse_expr(Lexer *lex);

#endif /* defined(__cppl__ast__) */
