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
	auto first = stream->get();
	
	for (;;) {
		switch (first) {
			case EOF: return Token(TOKEN_EOF);
			case '(': return Token(TOKEN_LPAREN);
			case ')': return Token(TOKEN_RPAREN);
			case '{': return Token(TOKEN_LBRACE);
			case '}': return Token(TOKEN_RBRACE);
			case ':': return Token(TOKEN_COLON);
			case ',': return Token(TOKEN_COMMA);
			case '=': return Token(TOKEN_EQ);
			case '"': {
				auto token = Token(TOKEN_STRING);
				auto chrs = new std::vector<char>;
				first = stream->get();
				while (first != '"') {
					if (first == '\\') {
						// TODO: Add special escape chars like \n and \r
						chrs->push_back(stream->get());
					} else {
						chrs->push_back(first);
					}
					// Read in the next character
					first = stream->get();
				}
				token._data.str_value = intern(chrs->data());
				return token;
			}
				
			// Skip newlines or whitespace
			case ' ':
			case '\n':
				continue;
				
			default: {
				// It's an identifier! Totally!
				auto token = Token(TOKEN_IDENT);
				auto chrs = new std::vector<char>;
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
					chrs->push_back(first);
					// Remove it from the input stream
					stream->get();
					// Get the next char on file!
					first = stream->peek();
				}
				token._data.ident = intern(chrs->data());
				if (strcmp(chrs->data(), "let") == 0) { // TODO: This is awful
					return Token(TOKEN_LET);
				}
				return token;
			}
		};
	}
	
}

Token Lexer::peek() {
	return cache;
}

Token Lexer::eat() {
	auto token = cache;
	cache = next_token();
	return token;
}

Token Lexer::expect(TokenType type) {
	auto token = eat();
	if (token.type() == type) {
		return token;
	} else {
		assert(false && "Unexpected token");
	}
}

bool Lexer::eof() {
	return cache.type() == TOKEN_EOF;
}
