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

aiff_container::aiff_container(uint8_t *raw_data)
{
    data = validate_aiff(raw_data);
    if (data == nullptr)
    {
        return;
    }
    uint32_t data_address = (uint32_t)data;

    // Check if the data is already aligned to 32 bytes
    if ((data_address & 31) == 0)
    {
        this->data = data;
    }
    else
    {
        // Align data to 32 bytes - this might cut off the last few bytes of the sound data
        uint32_t aligned_address = (data_address + 31) & ~31;

        // Move the data to the aligned address
        this->data = (aiff *)aligned_address;
        std::memmove(this->data, data, 8 + data->chunk_size);

        // Fix the chunk size
        uint32_t new_chunk_size = data->chunk_size + aligned_address - data_address;
        this->data->chunk_size = new_chunk_size;

        // Fix and align the sound data chunk size
        uint32_t new_sound_data_chunk_size = data->sound_data.chunk_size + aligned_address - data_address;
        this->data->sound_data.chunk_size = new_sound_data_chunk_size & ~31;
    }
}