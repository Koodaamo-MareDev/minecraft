#include "png_loader.hpp"
#include <miniz/miniz.h>
#include <gccore.h>

pnguin::PNGFile::PNGFile(const std::string &filename)
{
    this->filename = filename;
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file " + filename);
    }

    // Get file size and read the whole file into memory
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    ByteBuffer file_data;
    try
    {
        file_data.resize(file_size);
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for file " + filename);
    }

    file.read(reinterpret_cast<char *>(file_data.ptr()), file_size);
    file.close();

    // Check the header
    std::vector<uint8_t> header = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (file_data.size() < header.size() || std::memcmp(file_data.ptr(), header.data(), header.size()) != 0)
    {
        throw std::runtime_error("File " + filename + " is not a PNG file");
    }

    auto locate_next_chunk([&file_data](size_t start, const std::string &type) -> std::pair<size_t, size_t>
                           {
        size_t pos = start;
        while (pos + 12 <= file_data.size())
        {
            file_data.offset = pos;
            size_t chunk_size = file_data.readInt();
            if (pos + 12 + chunk_size > file_data.size())
            {
                break;
            }
            if (std::memcmp(file_data.ptr() + pos + 4, type.data(), 4) == 0)
            {
                return std::make_pair(pos + 8, chunk_size);
            }
            pos += 12 + chunk_size;
        }
        return std::make_pair(size_t(-1), size_t(0)); });

    auto verify_chunk_crc([&file_data](size_t pos, size_t chunk_size)
                          {
        file_data.offset = pos + chunk_size;
        uint32_t crc = file_data.readInt() ^ 0xFFFFFFFF;
        uint32_t calc_crc = mz_crc32(MZ_CRC32_INIT, file_data.ptr() + pos - 4, chunk_size + 4) ^ 0xFFFFFFFF;
        if (crc != calc_crc)
        {
            throw std::runtime_error("Invalid CRC for chunk");
        } });

    std::pair<size_t, size_t> ihdr = locate_next_chunk(8, "IHDR");
    if (ihdr.second != 13)
    {
        throw std::runtime_error("Invalid IHDR chunk size");
    }

    verify_chunk_crc(ihdr.first, ihdr.second);

    // Read IHDR data
    file_data.offset = ihdr.first;
    this->width = file_data.readInt();
    this->height = file_data.readInt();
    uint8_t bit_depth = file_data.readByte();
    uint8_t color_type = file_data.readByte();
    uint8_t compression_method = file_data.readByte();
    uint8_t filter_method = file_data.readByte();
    uint8_t interlace_method = file_data.readByte();

    // Check for read errors
    if (file_data.underflow)
    {
        throw std::runtime_error("Buffer underflow while reading IHDR");
    }

    // Only support 8-bit RGBA images
    if (bit_depth != 8 || color_type != 6 || compression_method != 0 || filter_method != 0 || interlace_method != 0)
    {
        throw std::runtime_error("Not a 8-bit RGBA image");
    }

    std::pair<size_t, size_t> idat = locate_next_chunk(ihdr.first + ihdr.second + 4, "IDAT");

    if (idat.second == 0)
    {
        throw std::runtime_error("IDAT chunk not found");
    }

    ByteBuffer compressed_data;

    // Locate the IDAT chunk
    while (true)
    {

        // Check for out of bounds pointer
        if (idat.first + idat.second > file_data.size())
        {
            throw std::runtime_error("Out of bounds read while inflating data");
        }

        // Copy the compressed data
        try
        {
            compressed_data.append(file_data.begin() + idat.first, file_data.begin() + idat.first + idat.second);
        }
        catch (std::bad_alloc &e)
        {
            throw std::runtime_error("Failed to allocate memory for compressed data");
        }

        idat = locate_next_chunk(idat.first + idat.second + 4, "IDAT");
        if (idat.second == 0)
        {
            break;
        }

        // Verify the IDAT chunk CRC
        verify_chunk_crc(idat.first, idat.second);
    }

    ByteBuffer decompressed_data;
    mz_ulong inflated_size = width * height * 4 + height;
    try
    {
        decompressed_data.resize(inflated_size);
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for image data");
    }
    int err;
    if ((err = mz_uncompress(decompressed_data.ptr(), &inflated_size, compressed_data.ptr(), compressed_data.size())) != MZ_OK)
    {
        throw std::runtime_error("Failed to inflate image data: " + std::string(mz_error(err)));
    }

    // Check for inflate errors
    if (inflated_size < width * height * 4 + height)
    {
        throw std::runtime_error("Failed to inflate all image data");
    }
    // Free the compressed data
    file_data.clear();

    // Copy the scanlines
    decompressed_data.offset = 0;
    try
    {
        pixels.resize(width * height * 4);
    }
    catch (std::bad_alloc &e)
    {
        throw std::runtime_error("Failed to allocate memory for image data");
    }
    uint8_t active_filter = 0;
    uint8_t r, g, b, a;
    uint8_t *pixels_data = pixels.ptr();
    for (uint32_t i = 0; i < height; i++)
    {
        // Get the filter byte
        active_filter = decompressed_data.readByte();
        switch (active_filter)
        {
        case 0:
        {
            // Copy the scanline
            std::memcpy(pixels.ptr() + i * width * 4, decompressed_data.ptr() + decompressed_data.offset, width * 4);
            decompressed_data.offset += width * 4;
            break;
        }
        case 1:
        {
            // Sub filter
            for (uint32_t j = 0; j < width; j++)
            {
                uint32_t curr = (i * width + j) << 2;
                r = decompressed_data.readByte();
                g = decompressed_data.readByte();
                b = decompressed_data.readByte();
                a = decompressed_data.readByte();
                if (j >= 1)
                {
                    uint32_t prev = (i * width + j - 1) << 2;
                    r = (int(r) + pixels_data[prev]) & 0xFF;
                    g = (int(g) + pixels_data[prev + 1]) & 0xFF;
                    b = (int(b) + pixels_data[prev + 2]) & 0xFF;
                    a = (int(a) + pixels_data[prev + 3]) & 0xFF;
                }
                pixels_data[curr] = r;
                pixels_data[curr + 1] = g;
                pixels_data[curr + 2] = b;
                pixels_data[curr + 3] = a;
            }
            break;
        }
        case 2:
        {
            // Up filter
            for (uint32_t j = 0; j < width; j++)
            {
                uint32_t curr = (i * width + j) << 2;
                r = decompressed_data.readByte();
                g = decompressed_data.readByte();
                b = decompressed_data.readByte();
                a = decompressed_data.readByte();
                if (i >= 1)
                {
                    uint32_t up = ((i - 1) * width + j) << 2;
                    r = (int(r) + pixels_data[up]) & 0xFF;
                    g = (int(g) + pixels_data[up + 1]) & 0xFF;
                    b = (int(b) + pixels_data[up + 2]) & 0xFF;
                    a = (int(a) + pixels_data[up + 3]) & 0xFF;
                }
                pixels_data[curr] = r;
                pixels_data[curr + 1] = g;
                pixels_data[curr + 2] = b;
                pixels_data[curr + 3] = a;
            }
            break;
        }
        case 3:
        {
            // Average filter
            for (uint32_t j = 0; j < width; j++)
            {
                uint32_t curr = (i * width + j) << 2;
                uint32_t prev = (j > 0) ? ((i * width + j - 1) << 2) : 0;
                uint32_t up = (i > 0) ? (((i - 1) * width + j) << 2) : 0;
                for (uint32_t k = 0; k < 4; k++) // Iterate over RGBA channels
                {
                    int a = (j > 0) ? pixels_data[prev + k] : 0;
                    int b = (i > 0) ? pixels_data[up + k] : 0;

                    pixels_data[curr + k] = (int(decompressed_data.readByte()) + ((a + b) / 2)) & 0xFF;
                }
            }
            break;
        }
        case 4:
        {
            // Paeth filter
            for (uint32_t j = 0; j < width; j++)
            {
                uint32_t curr = (i * width + j) << 2;
                uint32_t prev = (j > 0) ? ((i * width + j - 1) << 2) : 0;
                uint32_t up = ((i - 1) * width + j) << 2;
                uint32_t up_prev = (i > 0 && j > 0) ? (((i - 1) * width + j - 1) << 2) : 0;

                for (uint32_t k = 0; k < 4; k++) // Iterate over RGBA channels
                {
                    int a = (j > 0) ? pixels_data[prev + k] : 0;
                    int b = (i > 0) ? pixels_data[up + k] : 0;
                    int c = (i > 0 && j > 0) ? pixels_data[up_prev + k] : 0;

                    int p = a + b - c;
                    int pa = std::abs(p - a);
                    int pb = std::abs(p - b);
                    int pc = std::abs(p - c);

                    int pr;
                    if (pa <= pb && pa <= pc)
                        pr = a;
                    else if (pb <= pc)
                        pr = b;
                    else
                        pr = c;

                    pixels_data[curr + k] = (int(decompressed_data.readByte()) + pr) & 0xFF;
                }
            }
            break;
        }
        default:
            throw std::runtime_error("Unsupported filter type " + std::to_string(active_filter) + " in scanline " + std::to_string(i));
        }

        // Check for out of bounds read
        if (decompressed_data.underflow)
        {
            throw std::runtime_error("Out of bounds read while copying scanline");
        }
    }

    // Check for out of bounds read
    if (decompressed_data.offset != inflated_size)
    {
        throw std::runtime_error("Failed to read all image data");
    }
}

void pnguin::PNGFile::to_tpl(GXTexObj &texture, uint8_t mipmap_count)
{
    uint32_t tmp_width = width;
    uint32_t tmp_height = height;
    uint32_t total_mipmaps = mipmap_count;
    uint32_t tpl_size = width * height << 2;
    while (mipmap_count-- > 0)
    {
        tmp_width >>= 1;
        tmp_height >>= 1;
        tpl_size += (tmp_width) * (tmp_height) << 2;
    }
    uint8_t *res = new (std::align_val_t(32)) uint8_t[tpl_size];
    uint8_t *res_ptr = res;
    uint8_t *tmp_ptr = (uint8_t *)pixels.ptr();

    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
        {

            // Get the index to the 4x4 texel in the target texture
            int index = (width << 2) * (y & ~3) + ((x & ~3) << 4);

            // Put the data within the 4x4 texel into the target texture
            int index_within = ((x & 3) + ((y & 3) << 2)) << 1;

            res_ptr[index + index_within + 1] = *tmp_ptr++;
            res_ptr[index + index_within + 32] = *tmp_ptr++;
            res_ptr[index + index_within + 33] = *tmp_ptr++;
            res_ptr[index + index_within] = *tmp_ptr++;
        }
    }
    // If mipmaps are requested, generate them
    uint8_t *mip_ptr = res + (width * height << 2);
    for (uint32_t i = 1; i <= total_mipmaps; i++)
    {
        uint32_t mip_width = width >> i;
        uint32_t mip_height = height >> i;
        uint32_t mip_size = mip_width * mip_height << 2;

        // Check if the mipmap size is valid
        if (mip_width == 0 || mip_height == 0 || mip_size == 0)
            break;
        for (uint32_t y = 0; y < mip_height; y++)
        {
            for (uint32_t x = 0; x < mip_width; x++)
            {
                // Get the most opaque pixel
                uint8_t avg[4] = {0, 0, 0, 0};
                bool all_opaque = true;
                void *pixel_ptr = nullptr;
                for (uint32_t yy = 0; yy < 2; yy++)
                {
                    for (uint32_t xx = 0; xx < 2; xx++)
                    {
                        uint32_t src_x = (x << i) + xx;
                        uint32_t src_y = (y << i) + yy;
                        uint32_t src_index = (width * src_y + src_x) << 2;
                        uint8_t *ptr = &pixels.ptr()[src_index];
                        avg[0] += ptr[0] >> 2;
                        avg[1] += ptr[1] >> 2;
                        avg[2] += ptr[2] >> 2;
                        uint8_t alpha = ptr[3];
                        if (alpha >= avg[3])
                        {
                            avg[3] = alpha;
                            pixel_ptr = ptr;
                        }
                        if (alpha != 255)
                        {
                            all_opaque = false;
                        }
                    }
                }
                if (all_opaque)
                {
                    avg[3] = 255;
                    pixel_ptr = avg;
                }

                // Get the index to the 4x4 texel in the target texture
                int index = (mip_width << 2) * (y & ~3) + ((x & ~3) << 4);

                // Put the data within the 4x4 texel into the target texture
                int index_within = ((x & 3) + ((y & 3) << 2)) << 1;

                mip_ptr[index + index_within + 1] = ((uint8_t *)pixel_ptr)[0];
                mip_ptr[index + index_within + 32] = ((uint8_t *)pixel_ptr)[1];
                mip_ptr[index + index_within + 33] = ((uint8_t *)pixel_ptr)[2];
                mip_ptr[index + index_within] = ((uint8_t *)pixel_ptr)[3];
            }
        }
        mip_ptr += mip_size;
    }

    // Set the texture parameters
    DCFlushRange(res, tpl_size);
    GX_InitTexObj(&texture, res, width, height, GX_TF_RGBA8, GX_MIRROR, GX_MIRROR, GX_FALSE);
    if (total_mipmaps > 0)
        GX_InitTexObjLOD(&texture, GX_NEAR_MIP_LIN, GX_NEAR, 0, total_mipmaps - 1, -0.8f, GX_ENABLE, GX_DISABLE, GX_ANISO_1);
    else
        GX_InitTexObjFilterMode(&texture, GX_NEAR, GX_NEAR);
}
