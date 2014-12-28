//
//  ast.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include "ast.h"

/***********************
 * Misc Printing Stmts *
 ***********************/

std::ostream& operator<<(std::ostream& os, Type &ty) {
    return os << ty.ident;
}

std::ostream& operator<<(std::ostream& os, Argument &arg) {
    return os << arg.name << ": " << arg.type;
}

std::ostream& operator<<(std::ostream& os, OperationType opty) {
    switch (opty) {
    case OPERATION_PLUS:
        return os << "PLUS";
    case OPERATION_MINUS:
        return os << "MINUS";
    case OPERATION_TIMES:
        return os << "TIMES";
    case OPERATION_DIVIDE:
        return os << "DIVIDE";
    case OPERATION_MODULO:
        return os << "MODULO";
    }
}

std::ostream& operator<<(std::ostream& os, Expr& expr) {
    return expr.show(os);
}

std::ostream& operator<<(std::ostream& os, Stmt& stmt) {
    return stmt.show(os);
}

/***************
 * Expressions *
 ***************/

std::ostream& StringExpr::show(std::ostream& os) {
    return os << '"' << value << '"';
}

void StringExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }

std::ostream& CallExpr::show(std::ostream& os) {
    os << &callee << '(';
    bool first = true;
    for (auto i = args.begin(); i != args.end(); i++) {
        if (first) first = false; else os << ", ";
        os << **i;
    }
    return os << ')';
}

void CallExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }

std::ostream& IdentExpr::show(std::ostream& os) {
    return os << ident;
}

void IdentExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }

std::ostream& InfixExpr::show(std::ostream& os) {
    return os << '(' << *lhs << ' ' << op << ' ' << *rhs << ')';
}

void InfixExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }

/**************
 * Statements *
 **************/

std::ostream& DeclarationStmt::show(std::ostream &os) {
    return os << "let " << name << ": " << &type << " = " << &value;
}

void DeclarationStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }

std::ostream& FunctionStmt::show(std::ostream &os) {
    os << "let " << name << "(";
    bool first = true;
    for (auto i = arguments.begin(); i != arguments.end(); i++) {
        if (first) first = false; else os << ", ";
        os << *i;
    }
    os << "): " << return_type << " = {\n";
    for (auto i = body.begin(); i != body.end(); i++) {
        os << **i;
        os << ";\n";
    }
    return os << "}";
}

void FunctionStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }

std::ostream& ExprStmt::show(std::ostream &os) {
    return os << *expr;
}

void ExprStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }

std::ostream& EmptyStmt::show(std::ostream &os) {
    return os << ";PASS;";
}

void EmptyStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }
