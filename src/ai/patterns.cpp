// 3x3 neighbourhood key extraction (the table itself is generated: patterns_table.cpp).
#include "ai/patterns.h"

namespace ai {

uint16_t pattern_key(const go::Board& b, go::Point p, go::Color c)
{
    const int s = b.stride();
    const go::Point n[8] = {
        go::Point(p - s - 1), go::Point(p - s), go::Point(p - s + 1), go::Point(p + 1),
        go::Point(p + s + 1), go::Point(p + s), go::Point(p + s - 1), go::Point(p - 1)
    };
    unsigned key = 0;
    for(int i = 0; i < 8; ++i)
    {
        const go::Color v = b.at(n[i]);
        unsigned code;
        if(v == go::EMPTY)       code = 0;
        else if(v == go::BORDER) code = 3;
        else                     code = (v == c) ? 1u : 2u;
        key |= code << (2 * i);
    }
    return uint16_t(key);
}

}  // namespace ai
