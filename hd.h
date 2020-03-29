#ifndef _hd_h_
#define _hd_h_

#include "hex.h"

inline void hd(const char *p, size_t len) {
    while (len > 0) {
        std::cerr << "   ";
        size_t i;
        for (i = 0; i < 16 && i < len; ++i)
            std::cerr << ' ' << hex(p[i] & 0xff, 2, '0');
        for (; i < 16; ++i)
            std::cerr << "   ";
        std::cerr << "  ";
        for (i = 0; i < 16 && i < len; ++i) {
            char ch = p[i] & 0x7f;
            std::cerr << (ch < ' ' || ch == 127) ? '.' : ch; }
        std::cerr << std::endl;
        p += i;
        len -= i; }
}

#endif /*  _hd_h_ */
