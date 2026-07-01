#pragma once

#include "buffer.hpp"
#include <cstddef>

class BufferPass
{
public:
    VBO cached = VBO();
    VBO uncached = VBO();

    void refresh();
    bool is_same();
    size_t size();
    void clear();
};
