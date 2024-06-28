#include "aiff.hpp"

// 16000 Hz as 80-bit IEEE 754 floating point
const uint8_t sample_rate_bytes[10]{
    0x40, 0x0C, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

aiff *validate_aiff(uint8_t *data)
{
    aiff *a = (aiff *)data;

    if (std::strncmp(a->chunk_id, "FORM", 4) != 0)
    {
        return nullptr;
    }

    if (std::strncmp(a->form_type, "AIFF", 4) != 0)
    {
        return nullptr;
    }

    if (std::strncmp(a->common.chunk_id, "COMM", 4) != 0)
    {
        return nullptr;
    }

    if (a->common.chunk_size != 18)
    {
        return nullptr;
    }

    if (a->common.num_channels != 1)
    {
        return nullptr;
    }

    if (a->common.sample_size != 16)
    {
        return nullptr;
    }

    if (std::memcmp(a->common.sample_rate, sample_rate_bytes, 10) != 0)
    {
        return nullptr;
    }

    if (std::strncmp(a->sound_data.chunk_id, "SSND", 4) != 0)
    {
        return nullptr;
    }

    return a;
}