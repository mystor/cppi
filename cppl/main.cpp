//
//  main.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include <iostream>
#include <sstream>
#include "lexer.h"
#include "ast.h"

int main(int argc, const char * argv[]) {
	
	auto inputString = "let abc(def : int) : int = { 5 + 5 }";
	std::stringstream ss;
	ss << inputString;
	
	std::cout << "Running parser on: " << inputString << "\n";
	
	auto lex = Lexer(&ss);
	auto stmts = parse(&lex);
	
	for (auto i = stmts.begin(); i != stmts.end(); i++) {
		std::cout << **i << ";\n";
	}
	
    return 0;
}
