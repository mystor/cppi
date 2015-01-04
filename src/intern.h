//
//  intern.h
//  cppl
//
//  Created by Michael Layzell on 2014-12-22.
//  Copyright (c) 2014 Michael Layzell. All rights reserved.
//

#ifndef __cppl__intern__
#define __cppl__intern__

#include <iostream>
#include <string>

struct istr {
    const char *data;

    bool operator==(const istr &other) const {
        return data == other.data;
    }
};

std::ostream& operator<<(std::ostream& os, istr &s);

namespace std {
    template <>
    struct hash<istr> {
        std::size_t operator()(const istr &s) const {
            return hash<const char *>()(s.data);
        }
    };
}


istr intern(std::string str);


#endif /* defined(__cppl__intern__) */
