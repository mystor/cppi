//
//  lexer.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__lexer__
#define __cppl__lexer__

#include <iostream>
#include "intern.h"

enum TokenType {
    // ( ) and { }
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_TIMES,
    TOKEN_DIVIDE,
    TOKEN_MODULO,

    // : = and ;
    TOKEN_COLON,
    TOKEN_EQ,
    TOKEN_SEMI,
    TOKEN_COMMA,

    // Keywords
    TOKEN_LET,
    TOKEN_FN,

    // Other tokens!
    TOKEN_IDENT,
    TOKEN_STRING,
    TOKEN_INT,
    TOKEN_EOF
};

std::ostream& operator<<(std::ostream& os, TokenType n);

// An object representing a token
class Token {
    TokenType _type;
public:
    union {
        int int_value;
        const char *ident;
        const char *str_value;
    } _data;

    Token(TokenType type);
    TokenType type();
};

std::ostream& operator<<(std::ostream& os, Token n);

// The lexer object!
class Lexer {
    std::istream *stream;
    Token cache;

    Token next_token();
public:
    Lexer(std::istream *input);
    Token peek();
    Token eat();
    Token expect(TokenType type);
    bool eof();
};

#endif /* defined(__cppl__lexer__) */
