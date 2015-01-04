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

class Expr;
class Stmt;

class Type {
public:
    Type() { ident = intern("void"); };
    Type(istr ident) : ident(ident) {};
    istr ident; // interned
};
std::ostream& operator<<(std::ostream& os, Type &type);

const Type TYPE_NULL = Type(intern("void"));

class Argument {
public:
    Argument(istr name, Type type) : name(name), type(type) {};
    istr name;
    Type type;
};
std::ostream& operator<<(std::ostream& os, Argument &arg);

class FunctionProto {
public:
    FunctionProto(istr name, std::vector<Argument> arguments, Type return_type)
        : name(name), arguments(arguments), return_type(return_type) {};
    istr name;
    std::vector<Argument> arguments;
    Type return_type;
};

class Branch {
public:
    Branch(std::unique_ptr<Expr> cond, std::vector<std::unique_ptr<Stmt>> body)
        : cond(std::move(cond)), body(std::move(body)) {};
    std::unique_ptr<Expr> cond;
    std::vector<std::unique_ptr<Stmt>> body;
};

/***************
 * Expressions *
 ***************/
class ExprVisitor;
class Expr {
public:
    virtual std::ostream& show(std::ostream& os) = 0;
    virtual void accept(ExprVisitor &visitor) = 0;
};

std::ostream& operator<<(std::ostream& os, Expr &expr);

class StringExpr : public Expr {
public:
    StringExpr(istr value) : value(value) {};
    istr value; // Interned!
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ExprVisitor &visitor);
};

class IntExpr : public Expr {
public:
    IntExpr(int value) : value(value) {};
    int value;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ExprVisitor &visitor);
};

class BoolExpr : public Expr {
public:
    BoolExpr(bool value) : value(value) {};
    bool value;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ExprVisitor &visitor);
};

class CallExpr : public Expr {
public:
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), args(std::move(args)) {};
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ExprVisitor &visitor);
};

class IdentExpr : public Expr {
public:
    IdentExpr(istr ident) : ident(ident) {};
    istr ident;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ExprVisitor &visitor);
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
    virtual void accept(ExprVisitor &visitor);
};

class IfExpr : public Expr {
public:
    IfExpr(std::vector<Branch> branches) : branches(std::move(branches)) {};
    std::vector<Branch> branches;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ExprVisitor &visitor);
};

class ExprVisitor {
public:
    virtual void visit(StringExpr *expr) = 0;
    virtual void visit(IntExpr *expr) = 0;
    virtual void visit(BoolExpr *expr) = 0;
    virtual void visit(CallExpr *expr) = 0;
    virtual void visit(IdentExpr *expr) = 0;
    virtual void visit(InfixExpr *expr) = 0;
    virtual void visit(IfExpr *expr) = 0;
};

/**************
 * Statements *
 **************/

class StmtVisitor;
class Stmt {
public:
    virtual std::ostream& show(std::ostream& os) = 0;
    virtual void accept(StmtVisitor &visitor) = 0;
};

std::ostream& operator<<(std::ostream& os, Stmt &stmt);

class DeclarationStmt : public Stmt {
public:
    DeclarationStmt(istr name, Type type, std::unique_ptr<Expr> value) : name(name), type(type), value(std::move(value)) {};
    istr name;
    Type type;
    std::unique_ptr<Expr> value;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(StmtVisitor &visitor);
};

class ExprStmt : public Stmt {
public:
    ExprStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {};
    std::unique_ptr<Expr> expr;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(StmtVisitor &visitor);
};

class ReturnStmt : public Stmt {
public:
    ReturnStmt(std::unique_ptr<Expr> value) : value(std::move(value)) {};
    std::unique_ptr<Expr> value;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(StmtVisitor &visitor);
};

class EmptyStmt : public Stmt {
public:
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(StmtVisitor &visitor);
};
const EmptyStmt EMPTY_STMT = EmptyStmt();

class StmtVisitor {
public:
    virtual void visit(DeclarationStmt *stmt) = 0;
    virtual void visit(ExprStmt *stmt) = 0;
    virtual void visit(ReturnStmt *stmt) = 0;
    virtual void visit(EmptyStmt *stmt) = 0;
};


/*********
 * Items *
 *********/

class ItemVisitor;
class Item {
public:
    virtual std::ostream& show(std::ostream& os) = 0;
    virtual void accept(ItemVisitor &visitor) = 0;
};

std::ostream& operator<<(std::ostream& os, Item &stmt);

class FunctionItem : public Item {
public:
    FunctionItem(FunctionProto proto, std::vector<std::unique_ptr<Stmt>> body)
        : proto(proto), body(std::move(body)) {};
    FunctionProto proto;
    std::vector<std::unique_ptr<Stmt>> body;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ItemVisitor &visitor);
};

class StructItem : public Item {
public:
    StructItem(std::vector<Argument> args) : args(args) {};
    istr name;
    std::vector<Argument> args;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ItemVisitor &visitor);
};

class FFIFunctionItem : public Item {
public:
    FFIFunctionItem(FunctionProto proto) : proto(proto) {};
    FunctionProto proto;
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ItemVisitor &visitor);
};

class EmptyItem : public Item {
public:
    virtual std::ostream& show(std::ostream& os);
    virtual void accept(ItemVisitor &visitor);
};

const EmptyItem EMPTY_ITEM = EmptyItem();

class ItemVisitor {
public:
    virtual void visit(FunctionItem *item) = 0;
    virtual void visit(StructItem *item) = 0;
    virtual void visit(FFIFunctionItem *item) = 0;
    virtual void visit(EmptyItem *item) = 0;
};

#endif /* defined(__cppl__ast__) */
