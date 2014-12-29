#include <assert.h>
#include "parse.h"

std::vector<std::unique_ptr<Item>> parse_items(Lexer *lex) {
    std::vector<std::unique_ptr<Item>> items;
    while (! lex->eof()) {
        items.push_back(parse_item(lex));
    }
    return std::move(items);
}

std::vector<std::unique_ptr<Stmt>> parse_stmts(Lexer *lex) {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (! lex->eof()) {
        stmts.push_back(parse_stmt(lex));

        // Eat an intervening semicolon
        if (lex->peek().type() == TOKEN_SEMI) {
            lex->eat();
        } else {
            break;
        }
    }
    return std::move(stmts);
}

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

FunctionProto parse_function_proto(Lexer *lex) {
    lex->expect(TOKEN_FN);
    auto name = lex->expect(TOKEN_IDENT)._data.ident;
    lex->expect(TOKEN_LPAREN);

    std::vector<Argument> arguments;
    if (lex->peek().type() != TOKEN_RPAREN) {
        for (;;) {
            arguments.push_back(parse_argument(lex));
            if (lex->peek().type() == TOKEN_COMMA) {
                lex->eat();
            } else { break; }
        }
    }

    lex->expect(TOKEN_RPAREN);

    auto return_type = TYPE_NULL;
    if (lex->peek().type() == TOKEN_COLON) {
        lex->eat();
        return_type = parse_type(lex);
    }

    return FunctionProto(name, arguments, return_type);
}

std::unique_ptr<Item> parse_item(Lexer *lex) {
    auto first_type = lex->peek().type();
    switch (first_type) {
    case TOKEN_FN: {
        auto proto = parse_function_proto(lex);

        lex->expect(TOKEN_LBRACE);
        // The body of the function
        auto body = parse_stmts(lex);
        lex->expect(TOKEN_RBRACE);

        return std::make_unique<FunctionItem>(proto, std::move(body));
    } break;

    case TOKEN_FFI: {
        lex->eat();
        first_type = lex->peek().type();
        switch (first_type) {
        case TOKEN_FN: {
            auto proto = parse_function_proto(lex);
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

std::unique_ptr<Stmt> parse_stmt(Lexer *lex) {
    auto first_type = lex->peek().type();
    if (first_type == TOKEN_LET) { // TODO: Don't require the let in the future (it'll only take 1 token more lookahead)
        lex->eat();
        auto var = lex->expect(TOKEN_IDENT);
        lex->expect(TOKEN_COLON);
        // For now, we're requiring types EVERYWHERE!
        auto type = parse_type(lex);
        lex->expect(TOKEN_EQ);
        auto expr = parse_expr(lex);

        return std::make_unique<DeclarationStmt>(var._data.ident, type, std::move(expr));
    } else if (first_type == TOKEN_RETURN) {
        lex->eat();
        if (lex->peek().type() == TOKEN_SEMI) {
            return std::make_unique<ReturnStmt>(nullptr);
        } else {
            auto value = parse_expr(lex);
            return std::make_unique<ReturnStmt>(std::move(value));
        }
    } else if (first_type == TOKEN_SEMI || first_type == TOKEN_RBRACE) {
        return std::make_unique<EmptyStmt>();
    } else {
        auto expr = parse_expr(lex);
        return std::make_unique<ExprStmt>(std::move(expr));
    }
}


std::unique_ptr<Expr> parse_expr_val(Lexer *lex) {
    auto tok_type = lex->peek().type();

    switch (tok_type) {
    case TOKEN_STRING: {
        return std::make_unique<StringExpr>(lex->eat()._data.str_value);
    }
    case TOKEN_INT: {
        return std::make_unique<IntExpr>(lex->eat()._data.int_value);
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
        std::cerr << "Unexpected " << lex->peek() << ", not valid expr starter\n";
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

std::vector<std::unique_ptr<Item>> parse(Lexer *lex) {
    auto items = parse_items(lex);
    if (! lex->eof()) {
        std::cerr << "Unexpected " << lex->peek() << "; expected EOF\n";
        assert(false && "Expected end of file");
    }
    return std::move(items);
};
