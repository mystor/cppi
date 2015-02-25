#include <assert.h>
#include "parse.h"

std::vector<std::unique_ptr<Item>> parseItems(Lexer *lex) {
    std::vector<std::unique_ptr<Item>> items;
    while (! lex->eof()) {
        items.push_back(parseItem(lex));
    }
    return std::move(items);
}

std::vector<std::unique_ptr<Stmt>> parseStmts(Lexer *lex) {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (! lex->eof()) {
        stmts.push_back(parseStmt(lex));

        // Eat an intervening semicolon
        if (lex->peek().type == TOKEN_SEMI) {
            lex->eat();
        } else {
            break;
        }
    }
    return std::move(stmts);
}

Type parseType(Lexer *lex) {
    // TODO: Implement
    auto ident = lex->expect(TOKEN_IDENT).data.ident;
    return Type(ident);
}

Argument parseArgument(Lexer *lex) {
    auto ident = lex->expect(TOKEN_IDENT).data.ident;
    lex->expect(TOKEN_COLON);
    return Argument(ident, parseType(lex));
}

FunctionProto parseFunctionProto(Lexer *lex) {
    lex->expect(TOKEN_FN);
    auto name = lex->expect(TOKEN_IDENT).data.ident;
    lex->expect(TOKEN_LPAREN);

    std::vector<Argument> arguments;
    if (lex->peek().type != TOKEN_RPAREN) {
        for (;;) {
            arguments.push_back(parseArgument(lex));
            if (lex->peek().type == TOKEN_COMMA) {
                lex->eat();
            } else { break; }
        }
    }

    lex->expect(TOKEN_RPAREN);

    auto returnType = TYPE_NULL;
    if (lex->peek().type == TOKEN_COLON) {
        lex->eat();
        returnType = parseType(lex);
    }

    return FunctionProto(name, arguments, returnType);
}

std::unique_ptr<Item> parseItem(Lexer *lex) {
    auto firstType = lex->peek().type;
    switch (firstType) {
    case TOKEN_FN: {
        auto proto = parseFunctionProto(lex);

        lex->expect(TOKEN_LBRACE);
        // The body of the function
        auto body = parseStmts(lex);
        lex->expect(TOKEN_RBRACE);

        return std::make_unique<FunctionItem>(proto, std::move(body));
    } break;

    case TOKEN_FFI: {
        lex->eat();
        firstType = lex->peek().type;
        switch (firstType) {
        case TOKEN_FN: {
            auto proto = parseFunctionProto(lex);
            lex->expect(TOKEN_SEMI);

            return std::make_unique<FFIFunctionItem>(proto);
        } break;

        default: {
            std::cerr << "Unexpected " << lex->peek() << ", expected FN";
            assert(false && "Unexpected Token while parsing FFI");
        } break;
        }
    } break;

    case TOKEN_SEMI: {
        lex->eat();
        return std::make_unique<EmptyItem>();
    } break;

    default: {
        std::cerr << "Unexpected token " << lex->peek() << ". Expected `fn` or `;`\n";
        assert(false && "UNEXPECTED TOKEN");
    } break;
    };
}

std::unique_ptr<Stmt> parseStmt(Lexer *lex) {
    auto firstType = lex->peek().type;
    if (firstType == TOKEN_LET) { // TODO: Don't require the let in the future (it'll only take 1 token more lookahead)
        lex->eat();
        auto var = lex->expect(TOKEN_IDENT);
        lex->expect(TOKEN_COLON);
        // For now, we're requiring types EVERYWHERE!
        auto type = parseType(lex);
        lex->expect(TOKEN_EQ);
        auto expr = parseExpr(lex);

        return std::make_unique<DeclarationStmt>(var.data.ident, type, std::move(expr));
    } else if (firstType == TOKEN_RETURN) {
        lex->eat();
        if (lex->peek().type == TOKEN_SEMI) {
            return std::make_unique<ReturnStmt>(nullptr);
        } else {
            auto value = parseExpr(lex);
            return std::make_unique<ReturnStmt>(std::move(value));
        }
    } else if (firstType == TOKEN_SEMI || firstType == TOKEN_RBRACE) {
        return std::make_unique<EmptyStmt>();
    } else {
        auto expr = parseExpr(lex);
        return std::make_unique<ExprStmt>(std::move(expr));
    }
}

std::vector<Branch> parseIf(Lexer *lex) {
    lex->expect(TOKEN_IF);
    auto cond = parseExpr(lex);
    lex->expect(TOKEN_LBRACE);
    auto body = parseStmts(lex);
    lex->expect(TOKEN_RBRACE);

    if (lex->peek().type == TOKEN_ELSE) {
        lex->eat();
        if (lex->peek().type == TOKEN_IF) { // There is an else if { block }
            auto rest = parseIf(lex);
            rest.insert(rest.begin(), Branch(std::move(cond), std::move(body)));

            return rest;
        } else {                              // It has only an else { block }
            std::vector<Branch> branches;
            branches.push_back(Branch(std::move(cond), std::move(body)));
            lex->expect(TOKEN_LBRACE);
            auto elsBody = parseStmts(lex);
            lex->expect(TOKEN_RBRACE);
            branches.push_back(Branch(nullptr, std::move(elsBody)));

            return branches;
        }
    } else {
        std::vector<Branch> branches;
        branches.push_back(Branch(std::move(cond), std::move(body)));
        return branches;
    }
}

std::unique_ptr<Expr> parseExprVal(Lexer *lex) {
    auto tokType = lex->peek().type;

    switch (tokType) {
    case TOKEN_STRING: {
        return std::make_unique<StringExpr>(lex->eat().data.strValue);
    }
    case TOKEN_INT: {
        return std::make_unique<IntExpr>(lex->eat().data.intValue);
    }
    case TOKEN_TRUE: {
        lex->eat();
        return std::make_unique<BoolExpr>(true);
    }
    case TOKEN_FALSE: {
        lex->eat();
        return std::make_unique<BoolExpr>(false);
    }
    case TOKEN_IDENT: {
        return std::make_unique<IdentExpr>(lex->eat().data.ident);
    }
    case TOKEN_LPAREN: {
        lex->eat();
        auto expr = parseExpr(lex);
        lex->expect(TOKEN_RPAREN);
        return expr;
    }
    case TOKEN_IF: {
        auto branches = parseIf(lex);
        return std::make_unique<IfExpr>(std::move(branches));
    }
    default: {
        std::cerr << "Unexpected " << lex->peek() << ", not valid expr starter\n";
        assert(false && "Unrecognized expression");
    }
    }
}

std::unique_ptr<Expr> parseExprAccess(Lexer *lex) {
    auto expr = parseExprVal(lex);
    for (;;) {
        switch (lex->peek().type) {
        case TOKEN_LPAREN: {
            // Parse some args!
            lex->eat();
            std::vector<std::unique_ptr<Expr>> args;
            for (;;) {
                if (lex->peek().type == TOKEN_RPAREN) {
                    break;
                }
                args.push_back(parseExpr(lex));
                switch (lex->peek().type) {
                case TOKEN_COMMA:
                    lex->eat();
                    continue;
                case TOKEN_RPAREN:
                    break;
                default:
                    assert(false && "Expected comma or rparen after argument");
                }
                break;
            }
            lex->expect(TOKEN_RPAREN);
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
            continue;
        }
            // case TOKEN_DOT: // Property accessing!
        default: break;
        }
        break;
    }
    return expr;
}

std::unique_ptr<Expr> parseExprTdm(Lexer *lex) {
    auto expr = parseExprAccess(lex);
    for (;;) {
        switch (lex->peek().type) {
        case TOKEN_TIMES: {
            lex->eat();
            auto rhs = parseExprAccess(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_TIMES, std::move(expr), std::move(rhs));
        } continue;
        case TOKEN_DIVIDE: {
            lex->eat();
            auto rhs = parseExprAccess(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_DIVIDE, std::move(expr), std::move(rhs));
        } continue;
        case TOKEN_MODULO: {
            lex->eat();
            auto rhs = parseExprAccess(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_MODULO, std::move(expr), std::move(rhs));
        } continue;
        default: break;
        }
        break;
    }
    return expr;
}

std::unique_ptr<Expr> parseExprPm(Lexer *lex) {
    auto expr = parseExprTdm(lex);
    for (;;) {
        switch (lex->peek().type) {
        case TOKEN_PLUS: {
            lex->eat();
            auto rhs = parseExprAccess(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_PLUS, std::move(expr), std::move(rhs));
        } continue;
        case TOKEN_DIVIDE: {
            lex->eat();
            auto rhs = parseExprAccess(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_MINUS, std::move(expr), std::move(rhs));
        } continue;
        default: break;
        }
        break;
    }
    return expr;
}

std::unique_ptr<Expr> parseExpr(Lexer *lex) {
    return parseExprPm(lex);
}

std::vector<std::unique_ptr<Item>> parse(Lexer *lex) {
    auto items = parseItems(lex);
    if (! lex->eof()) {
        std::cerr << "Unexpected " << lex->peek() << "; expected EOF\n";
        assert(false && "Expected end of file");
    }
    return std::move(items);
};
