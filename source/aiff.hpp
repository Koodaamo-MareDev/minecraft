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
    unsigned char* bin_data = nullptr;
    aiff *data = nullptr;
    aiff_container(uint8_t *raw_data);
};

aiff *validate_aiff(uint8_t *data);

#endif