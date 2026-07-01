#include "buffer_pass.hpp"

void BufferPass::refresh()
{
    if (cached != uncached)
    {
        cached.clear();
        cached = uncached;
    }
}

bool BufferPass::is_same()
{
    return uncached == cached;
}

size_t BufferPass::size()
{
    size_t base_size = sizeof(*this);
    if (uncached.buffer == cached.buffer)
    {
        return base_size + cached.length;
    }
    return base_size + cached.length + uncached.length;
}

void BufferPass::clear()
{
    if (!is_same())
        uncached.clear();
    cached.clear();
}
