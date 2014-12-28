#include <assert.h>
#include "parse.h"

std::vector<std::unique_ptr<Stmt>> parse_stmts(Lexer *lex) {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (! lex->eof()) {
        stmts.push_back(parse_stmt(lex));
        // Remove a semicolon!
        if (lex->peek().type() == TOKEN_SEMI) {
            lex->eat();
        } else {
            break;
        }
    }
    return std::move(stmts);
}

std::vector<std::unique_ptr<Stmt>> parse(Lexer *lex) {
    auto stmts = parse_stmts(lex);
    assert(lex->eof() && "Expected end of file"); // TODO: Improve
    return std::move(stmts);
};

Type parse_type(Lexer *lex) {
    // TODO: Implement
    auto ident = lex->expect(TOKEN_IDENT)._data.ident;
    return Type(ident);
}

Argument parse_argument(Lexer *lex) {
    auto ident = lex->expect(TOKEN_IDENT)._data.ident;
    lex->expect(TOKEN_COLON);
    return Argument(ident, parse_type(lex));
}

std::unique_ptr<Stmt> parse_stmt(Lexer *lex) {
    auto first_type = lex->peek().type();
    if (first_type == TOKEN_LET) {
        lex->eat();
        auto var = lex->expect(TOKEN_IDENT);

        auto next = lex->eat();
        if (next.type() == TOKEN_LPAREN) {
            // Function declaration!
            // First, we parse the arguments
            std::vector<Argument> arguments;
            for (;;) {
                arguments.push_back(parse_argument(lex));
                if (lex->peek().type() == TOKEN_COMMA) {
                    lex->eat();
                    continue;
                } else { break; }
            }
            lex->expect(TOKEN_RPAREN);
            lex->expect(TOKEN_COLON);
            // Optionally do a return type
            Type type = lex->peek().type() == TOKEN_COLON ? TYPE_NULL : parse_type(lex);
            lex->expect(TOKEN_EQ);
            lex->expect(TOKEN_LBRACE);
            // Function body
            auto body = parse_stmts(lex);
            lex->expect(TOKEN_RBRACE);

            // Create the statement! (There should probably be a constructor...)
            auto name = var._data.ident;
            return std::make_unique<FunctionStmt>(name, std::move(arguments), type, std::move(body));
        } else if (next.type() == TOKEN_COLON) {
            return std::make_unique<DeclarationStmt>(var._data.ident, TYPE_NULL, parse_expr(lex));
        } else {
            assert(false && "Expected ( or :");
        }
    } else if (first_type == TOKEN_SEMI || first_type == TOKEN_RBRACE) {
        return std::make_unique<EmptyStmt>();
    } else {
        return std::make_unique<ExprStmt>(parse_expr(lex));
    }
}


std::unique_ptr<Expr> parse_expr_val(Lexer *lex) {
    auto tok_type = lex->peek().type();

    switch (tok_type) {
    case TOKEN_STRING: {
        return std::make_unique<StringExpr>(lex->eat()._data.str_value);
    }
    case TOKEN_IDENT: {
        return std::make_unique<IdentExpr>(lex->eat()._data.ident);
    }
    case TOKEN_LPAREN: {
        lex->eat();
        auto expr = parse_expr(lex);
        lex->expect(TOKEN_RPAREN);
        return expr;
    }
    default: {
        assert(false && "Unrecognized expression");
    }
    }
}

std::unique_ptr<Expr> parse_expr_access(Lexer *lex) {
    auto expr = parse_expr_val(lex);
    for (;;) {
        switch (lex->peek().type()) {
        case TOKEN_LPAREN: {
            // Parse some args!
            lex->eat();
            std::vector<std::unique_ptr<Expr>> args;
            for (;;) {
                if (lex->peek().type() == TOKEN_RPAREN) {
                    break;
                }
                args.push_back(parse_expr(lex));
                switch (lex->peek().type()) {
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

std::unique_ptr<Expr> parse_expr_tdm(Lexer *lex) {
    auto expr = parse_expr_access(lex);
    for (;;) {
        switch (lex->peek().type()) {
        case TOKEN_TIMES: {
            lex->eat();
            auto rhs = parse_expr_access(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_TIMES, std::move(expr), std::move(rhs));
        } continue;
        case TOKEN_DIVIDE: {
            lex->eat();
            auto rhs = parse_expr_access(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_DIVIDE, std::move(expr), std::move(rhs));
        } continue;
        case TOKEN_MODULO: {
            lex->eat();
            auto rhs = parse_expr_access(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_MODULO, std::move(expr), std::move(rhs));
        } continue;
        default: break;
        }
        break;
    }
    return expr;
}

std::unique_ptr<Expr> parse_expr_pm(Lexer *lex) {
    auto expr = parse_expr_tdm(lex);
    for (;;) {
        switch (lex->peek().type()) {
        case TOKEN_PLUS: {
            lex->eat();
            auto rhs = parse_expr_access(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_PLUS, std::move(expr), std::move(rhs));
        } continue;
        case TOKEN_DIVIDE: {
            lex->eat();
            auto rhs = parse_expr_access(lex);
            expr = std::make_unique<InfixExpr>(OPERATION_MINUS, std::move(expr), std::move(rhs));
        } continue;
        default: break;
        }
        break;
    }
    return expr;
}

std::unique_ptr<Expr> parse_expr(Lexer *lex) {
    return parse_expr_pm(lex);
}
