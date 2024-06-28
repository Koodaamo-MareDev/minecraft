#ifndef AIFF_HPP
#define AIFF_HPP

#include <cstdint>
#include <cstring>

struct common_chunk
{
    char chunk_id[4];
    int32_t chunk_size;
    int16_t num_channels;
    uint32_t num_sample_frames;
    int16_t sample_size;
    uint8_t sample_rate[10];
}__attribute__((packed, aligned(2)));

struct sound_data_chunk
{
    char chunk_id[4];
    int32_t chunk_size;
    int8_t sound_data[];
}__attribute__((packed, aligned(2)));

struct aiff
{
    char chunk_id[4];
    int32_t chunk_size;
    char form_type[4];
    common_chunk common;
    sound_data_chunk sound_data;
}__attribute__((packed, aligned(2)));

class aiff_container
{
public:
    aiff *data = nullptr;
    aiff_container(aiff *data)
    {
        if(data == nullptr)
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
};

aiff *validate_aiff(uint8_t *data);

#endif