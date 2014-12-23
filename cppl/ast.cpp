//
//  ast.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include "ast.h"
#include <assert.h>

ExprStmt::ExprStmt(Expr expr) : expr(expr) {
}

std::vector<Stmt> parse_stmts(Lexer *lex) {
	std::vector<Stmt> stmts;
	while (! lex->eof()) {
		stmts.push_back(parse_stmt(lex));
		// Remove a semicolon!
		if (lex->peek().type() == TOKEN_SEMI) {
			lex->eat();
		} else {
			break;
		}
	}
	return stmts;
}

std::vector<Stmt> parse(Lexer *lex) {
	auto stmts = parse_stmts(lex);
	assert(lex->eof() && "Expected end of file"); // TODO: Improve
	return stmts;
};

Type parse_type(Lexer *lex) {
	// TODO: Implement
	return Type();
}

Argument parse_argument(Lexer *lex) {
	Argument arg;
	auto ident = lex->expect(TOKEN_IDENT)._data.ident;
	lex->expect(TOKEN_COLON);
	arg.type = parse_type(lex);
	arg.name = ident;
	return arg;
}

Stmt parse_stmt(Lexer *lex) {
	auto first_type = lex->peek().type();
	if (first_type == TOKEN_LET) {
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
			Type type;
			if (lex->peek().type() == TOKEN_COLON) {
				type = TYPE_NULL;
			} else {
				type = parse_type(lex);
			}
			lex->expect(TOKEN_EQ);
			lex->expect(TOKEN_LBRACE);
			// Function body
			auto body = parse_stmts(lex);
			lex->expect(TOKEN_RBRACE);

			// Create the statement! (There should probably be a constructor...)
			FunctionStmt stmt;
			stmt.name = var._data.ident;
			stmt.return_type = type;
			stmt.arguments = arguments;
			stmt.body = body;
			return stmt;
		} else if (next.type() == TOKEN_COLON) {
			// Variable declaration!
			DeclarationStmt stmt;
			stmt.name = var._data.ident;
			stmt.value = parse_expr(lex);
			return stmt;
		} else {
			assert(false && "Expected ( or :");
		}
	} else if (first_type == TOKEN_SEMI || first_type == TOKEN_RBRACE) {
		return EMPTY_STMT;
	} else {
		return ExprStmt(parse_expr(lex));
	}
}


Expr parse_expr_val(Lexer *lex) {
	auto tok_type = lex->peek().type();
	
	switch (tok_type) {
		case TOKEN_STRING: {
			return StringExpr(lex->eat()._data.str_value);
		}
		case TOKEN_IDENT: {
			return IdentExpr(lex->eat()._data.ident);
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

Expr parse_expr_access(Lexer *lex) {
	Expr expr = parse_expr_val(lex);
	for (;;) {
		switch (lex->peek().type()) {
			case TOKEN_LPAREN: {
				// Parse some args!
				lex->eat();
				std::vector<Expr> args;
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
				expr = CallExpr(expr, args);
				continue;
			}
			// case TOKEN_DOT: // Property accessing!
			default: break;
		}
		break;
	}
	return expr;
}

Expr parse_expr(Lexer *lex) {
	return parse_expr_access(lex);
}