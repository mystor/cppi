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

std::ostream& operator<<(std::ostream& os, Item& item) {
    return item.show(os);
}

/***************
 * Expressions *
 ***************/

std::ostream& StringExpr::show(std::ostream& os) {
    return os << '"' << value << '"';
}

void StringExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& IntExpr::show(std::ostream& os) {
    return os << value;
}

void IntExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& BoolExpr::show(std::ostream& os) {
    return os << value;
}

void BoolExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& MkExpr::show(std::ostream& os) {
    os << "mk " << type << '{';
    bool first = true;
    for (auto i = fields.begin(); i != fields.end(); i++) {
        if (first) first = false; else os << ", ";
        os << **i;
    }
    return os << '}';
}

void MkExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& CallExpr::show(std::ostream& os) {
    os << *callee << '(';
    bool first = true;
    for (auto i = args.begin(); i != args.end(); i++) {
        if (first) first = false; else os << ", ";
        os << **i;
    }
    return os << ')';
}

void CallExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& MthdCallExpr::show(std::ostream& os) {
    os << *object << '.' << symbol << '(';
    bool first = true;
    for (auto i = args.begin(); i != args.end(); i++) {
        if (first) first = false; else os << ", ";
        os << **i;
    }
    return os << ')';
}

void MthdCallExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& MemberExpr::show(std::ostream& os) {
    return os << *object << '.' << symbol;
}

void MemberExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& IdentExpr::show(std::ostream& os) {
    return os << ident;
}

void IdentExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& InfixExpr::show(std::ostream& os) {
    return os << '(' << *lhs << ' ' << op << ' ' << *rhs << ')';
}

void InfixExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }


std::ostream& IfExpr::show(std::ostream& os) {
    auto first = true;
    for (auto &branch : branches) {
        if (first) first = false; else os << " else ";

        if (branch.cond != nullptr) {
            os << "if (" << *branch.cond << ") {\n";
        } else {
            os << "{\n";
        }

        for (auto &stmt : branch.body) {
            os << *stmt;
        }
        os << "}";
    }
    return os;
}

void IfExpr::accept(ExprVisitor &visitor) { visitor.visit(this); }

/**************
 * Statements *
 **************/

std::ostream& DeclarationStmt::show(std::ostream &os) {
    return os << "let " << name << ": " << type << " = " << *value << ";\n";
}

void DeclarationStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }


std::ostream& ExprStmt::show(std::ostream &os) {
    return os << *expr << ";\n";
}

void ExprStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }


std::ostream& ReturnStmt::show(std::ostream &os) {
    if (value == nullptr) {
        return os << "return VOID;\n";
    } else {
        return os << "return " << *value << ";\n";
    }
}

void ReturnStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }


std::ostream& EmptyStmt::show(std::ostream &os) {
    return os << "PASS;\n";
}

void EmptyStmt::accept(StmtVisitor &visitor) { visitor.visit(this); }

/*********
 * Items *
 *********/

std::ostream& FunctionItem::show(std::ostream &os) {
    os << "fn " << proto.name << "(";
    bool first = true;
    for (auto i = proto.arguments.begin(); i != proto.arguments.end(); i++) {
        if (first) first = false; else os << ", ";
        os << *i;
    }
    os << "): " << proto.returnType << " {\n";
    for (auto i = body.begin(); i != body.end(); i++) {
        os << **i;
    }
    return os << "}";
}

void FunctionItem::accept(ItemVisitor &visitor) { visitor.visit(this); }

std::ostream& StructItem::show(std::ostream &os) {
    os << "struct " << name << " {\n";
    bool first = true;
    for (auto i = args.begin(); i != args.end(); i++) {
        if (first) first = false; else os << ";\n";
        os << "  " << *i;
    }
    return os << "};\n";
}

void StructItem::accept(ItemVisitor &visitor) { visitor.visit(this); }

std::ostream& FFIFunctionItem::show(std::ostream &os) {
    os << "FFI fn " << proto.name << "(";
    bool first = true;
    for (auto i = proto.arguments.begin(); i != proto.arguments.end(); i++) {
        if (first) first = false; else os << ", ";
        os << *i;
    }
    return os << "): " << proto.returnType << ";";
}

void FFIFunctionItem::accept(ItemVisitor &visitor) { visitor.visit(this); }

std::ostream& EmptyItem::show(std::ostream &os) {
    return os << "PASS;";
}

void EmptyItem::accept(ItemVisitor &visitor) { visitor.visit(this); }
