#ifndef AIFF_HPP
#define AIFF_HPP

#include <cstdint>
#include <cstring>

struct CommonChunk
{
    char chunk_id[4];
    int32_t chunk_size;
    int16_t num_channels;
    uint32_t num_sample_frames;
    int16_t sample_size;
    uint8_t sample_rate[10];
}__attribute__((packed, aligned(2)));

struct SoundDataChunk
{
    char chunk_id[4];
    int32_t chunk_size;
    int8_t sound_data[];
}__attribute__((packed, aligned(2)));

struct Aiff
{
    char chunk_id[4];
    int32_t chunk_size;
    char form_type[4];
    CommonChunk common;
    SoundDataChunk sound_data;
}__attribute__((packed, aligned(2)));

class AiffContainer
{
public:
    unsigned char* bin_data = nullptr;
    Aiff *data = nullptr;
    AiffContainer(uint8_t *raw_data);
};

Aiff *validate_aiff(uint8_t *data);

#endif