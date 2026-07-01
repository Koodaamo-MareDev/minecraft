#include "buffer.hpp"

uint8_t *buffer = nullptr;
uint32_t length = 0;

VBO::VBO() : buffer(nullptr), length(0)
{
}

VBO::VBO(uint8_t *buffer, uint32_t length) : buffer(buffer), length(length)
{
}

bool VBO::operator==(VBO &other)
{
    return this->buffer == other.buffer && this->length == other.length;
}

bool VBO::operator!=(VBO &other)
{
    return this->buffer != other.buffer || this->length != other.length;
}

VBO::operator bool()
{
    return this->buffer != nullptr && this->length > 0;
}

void VBO::detach()
{
    this->buffer = nullptr;
    this->length = 0;
}

void VBO::clear()
{
    if (this->buffer)
    {
        delete[] this->buffer;
        this->buffer = nullptr;
    }
    this->length = 0;
}
