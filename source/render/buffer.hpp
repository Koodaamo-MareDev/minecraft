#pragma once

#include <cstdint>

class VBO
{
public:
    uint8_t *buffer = nullptr;
    uint32_t length = 0;

    VBO();

    VBO(uint8_t *buffer, uint32_t length);

    bool operator==(VBO &other);

    bool operator!=(VBO &other);

    operator bool();

    // Detach the buffer from the object without freeing it
    void detach();

    // Free the buffer and set the object to an empty state
    void clear();
};