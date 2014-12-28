//
//  intern.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include "intern.h"
#include <unordered_set>

// TODO: This sucks balls right now, should be improved
std::unordered_set<std::string> string_pool;
const char *intern(std::string string) {
    auto interned = string_pool.find(string);
    if (interned == string_pool.end()) {
        string_pool.insert(string);
        return string_pool.find(string)->data();
    } else {
        return interned->data();
    }
}
