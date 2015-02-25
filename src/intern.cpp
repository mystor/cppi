//
//  intern.cpp
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#include "intern.h"
#include <unordered_set>
#include <assert.h>

std::ostream& operator<<(std::ostream& os, istr &s) {
    return os << s.data;
}

// TODO: This sucks balls right now, should be improved
std::unordered_set<std::string> string_pool;
istr intern(std::string string) {
    auto interned = string_pool.find(string);
    if (interned == string_pool.end()) {
        string_pool.insert(string);
        interned = string_pool.find(string);
    }

    return { interned->data(), interned->length() };
}

// istr::istr() : length(0) { data = istr("").data; } // TODO: This could probably be done more efficiently
