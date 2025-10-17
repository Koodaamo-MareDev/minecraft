#include "aiff.hpp"
#include <new>
#include <stdexcept>

// Common frequencies as 80-bit IEEE 754 floating point
// 16000 Hz
const uint8_t sample_rate_16k_bytes[10]{0x40, 0x0C, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// 32000 Hz
const uint8_t sample_rate_32k_bytes[10]{0x40, 0x0D, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// 48000 Hz
const uint8_t sample_rate_48k_bytes[10]{0x40, 0x0E, 0xBB, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Aiff *validate_aiff(uint8_t *data)
{
    Aiff *a = (Aiff *)data;

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

    if (a->common.sample_size != 16 && a->common.sample_size != 8)
    {
        return nullptr;
    }

    if (std::memcmp(a->common.sample_rate, sample_rate_16k_bytes, 10) != 0 &&
        std::memcmp(a->common.sample_rate, sample_rate_32k_bytes, 10) != 0 &&
        std::memcmp(a->common.sample_rate, sample_rate_48k_bytes, 10) != 0)
    {
        return nullptr;
    }

    if (std::strncmp(a->sound_data.chunk_id, "SSND", 4) != 0)
    {
        return nullptr;
    }

    return a;
}

AiffContainer::AiffContainer(uint8_t *raw_data)
{
    Aiff *aiff_file = validate_aiff(raw_data);
    if (aiff_file == nullptr)
    {
        throw std::runtime_error("Unsupported or invalid AIFF file");
    }
    // Look up the sample rate from the supported values
    if (memcmp(aiff_file->common.sample_rate, sample_rate_16k_bytes, 10) == 0)
        sample_rate = 16000;
    else if (memcmp(aiff_file->common.sample_rate, sample_rate_32k_bytes, 10) == 0)
        sample_rate = 32000;
    else if (memcmp(aiff_file->common.sample_rate, sample_rate_48k_bytes, 10) == 0)
        sample_rate = 48000;

    // Look up the bit depth from the common chunk
    sample_format = 0; // Default to 8-bit
    if (aiff_file->common.sample_size == 16)
    {
        this->sample_format |= 1; // 16-bit
    }
    if (aiff_file->common.num_channels == 2)
    {
        this->sample_format |= 2; // Stereo
    }

    data = new (std::align_val_t(32), std::nothrow) uint8_t[aiff_file->sound_data.chunk_size];
    if (!data)
    {
        // Allocation failed
        throw std::runtime_error("Failed to allocate memory for AIFF sound data");
    }
    std::memcpy(data, aiff_file->sound_data.data, aiff_file->sound_data.chunk_size);
    data_size = aiff_file->sound_data.chunk_size;
}