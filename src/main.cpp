//
//  main.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.h"
#include "ast.h"
#include "parse.h"

int main(int argc, const char * argv[]) {
    // Usage Message (TODO: Improve)
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <FileName>\n"
                  << "Compiles the file given by <FileName>\n\n";
        return -1;
    }

    // Read in the file passed as the first argument
    std::ifstream file_stream;
    file_stream.open(argv[1]);

    // Parse it!
    auto lex = Lexer(&file_stream);
    auto stmts = parse(&lex);

    std::cout << "Result of parsing: \n";
    for (auto &stmt : stmts) {
        std::cout << *stmt << ";\n";
    }

    // TODO: Actually compile the code!

    return 0;
}
