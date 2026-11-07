#include "table.h"
#include <string.h>

ElementSymbol get_element_symbol(const char* str) {
    #define X(value, ox, count, elect, type) \
        if (strcmp(str, #value) == 0) return value;
        SYMBOLS
    #undef X
    
    return UNKNOWN_ELEMENT;
}

const char *symbol_to_string(ElementSymbol sym) {
    switch (sym) {
        #define X(value, ox, count, elect, type) \
        case value: return #value;
            SYMBOLS
        #undef X
        default:
            return "UNDEFINED";
    }
}

