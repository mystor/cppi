//
//  lexer.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include "lexer.h"
#include <vector>
#include <assert.h>
#include "intern.h"

Lexer::Lexer(std::istream *input) : cache(TOKEN_EOF), stream(input) {
	cache = next_token();
}

Token::Token(TokenType type) {
	_type = type;
}

Token::~Token() {
	
}

TokenType Token::type() {
	return _type;
}

// Let's do some lexing!

Token Lexer::next_token() {
	for (;;) {
		auto first = stream->get();
		switch (first) {
			case EOF: return Token(TOKEN_EOF);
			case '(': return Token(TOKEN_LPAREN);
			case ')': return Token(TOKEN_RPAREN);
			case '{': return Token(TOKEN_LBRACE);
			case '}': return Token(TOKEN_RBRACE);
			case ':': return Token(TOKEN_COLON);
			case ',': return Token(TOKEN_COMMA);
			case '=': return Token(TOKEN_EQ);
				
			case '+': return Token(TOKEN_PLUS);
			case '-': return Token(TOKEN_MINUS);
			case '*': return Token(TOKEN_TIMES);
			case '/': return Token(TOKEN_DIVIDE);
			case '%': return Token(TOKEN_MODULO);
				
			case '"': {
				auto token = Token(TOKEN_STRING);
				std::string chrs;
				first = stream->get();
				while (first != '"') {
					if (first == '\\') {
						// TODO: Add special escape chars like \n and \r
						chrs.push_back(stream->get());
					} else {
						chrs.push_back(first);
					}
					// Read in the next character
					first = stream->get();
				}
				token._data.str_value = intern(chrs);
				return token;
			}
				
			// Skip newlines or whitespace
			case ' ':
			case '\n':
				break;
				
			default: {
				// It's an identifier! Totally!
				auto token = Token(TOKEN_IDENT);
				std::string chrs;
				chrs.push_back(first);
				first = stream->peek();
				while (first != EOF &&
					   first != '(' &&
					   first != ')' &&
					   first != '{' &&
					   first != '}' &&
					   first != ':' &&
					   first != '=' &&
					   first != '"' &&
					   first != ' ' &&
					   first != '\n') {
					// Record this character
					chrs.push_back(first);
					// Remove it from the input stream
					stream->get();
					// Get the next char on file!
					first = stream->peek();
				}
				token._data.ident = intern(chrs);
				if (chrs == "let") { // TODO: This is awful
					return Token(TOKEN_LET);
				}
				return token;
			}
		};
	}
	
}

Token Lexer::peek() {
	std::cout << "Looking at " << cache << "\n";
	return cache;
}

Token Lexer::eat() {
	auto token = cache;
	cache = next_token();
	std::cout << "Ate a " << cache << "\n";
	return token;
}

Token Lexer::expect(TokenType type) {
	auto token = eat();
	if (token.type() == type) {
		return token;
	} else {
		std::cerr << "Unexpected token " << token << " expected " << type << "\n";
		assert(false && "Unexpected Token");
	}
}

bool Lexer::eof() {
	if (cache.type() == TOKEN_EOF) {
		return true;
	} else {
		std::cout << "Instead saw a " << cache << "\n";
		return false;
	}
}

std::ostream& operator<<(std::ostream& os, TokenType n) {
	switch (n) {
		case TOKEN_LPAREN: {
			os << "LPAREN";
		} break;
		case TOKEN_RPAREN: {
			os << "RPAREN";
		} break;
		case TOKEN_LBRACE: {
			os << "LBRACE";
		} break;
		case TOKEN_RBRACE: {
			os << "RBRACE";
		} break;
		case TOKEN_COLON: {
			os << "COLON";
		} break;
		case TOKEN_EQ: {
			os << "EQ";
		} break;
		case TOKEN_SEMI: {
			os << "SEMI";
		} break;
		case TOKEN_COMMA: {
			os << "COMMA";
		} break;
		case TOKEN_LET: {
			os << "LET";
		} break;
		case TOKEN_IDENT: {
			os << "IDENT";
		} break;
		case TOKEN_STRING: {
			os << "IDENT";
		} break;
		case TOKEN_EOF: {
			os << "EOF";
		} break;
		case TOKEN_PLUS: {
			os << "PLUS";
		} break;
		case TOKEN_MINUS: {
			os << "MINUS";
		} break;
		case TOKEN_TIMES: {
			os << "TIMES";
		} break;
		case TOKEN_DIVIDE: {
			os << "DIVIDE";
		} break;
		case TOKEN_MODULO: {
			os << "MODULO";
		} break;
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, Token n) {
	os << "Token(" << n.type();
	switch (n.type()) {
		case TOKEN_IDENT:
			os << " " << n._data.ident;
			break;
		case TOKEN_STRING:
			os << " " << n._data.str_value;
			break;
		default:
			break;
	}
	return os << ")";
}
