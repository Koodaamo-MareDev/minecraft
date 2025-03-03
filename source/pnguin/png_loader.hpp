#ifndef PNG_LOADER_HPP
#define PNG_LOADER_HPP

#include <stdexcept>
#include <cstring>
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <ported/ByteBuffer.hpp>

#include <ogc/gx.h>

namespace pnguin
{
    class PNGFile
    {
        std::string filename;
        ByteBuffer pixels;
        uint32_t width = 0, height = 0;

    public:
        PNGFile(const std::string &filename);
        uint32_t get_width() const { return width; }
        uint32_t get_height() const { return height; }
        const uint8_t *get_data() const { return pixels.data.data(); }
        void to_tpl(GXTexObj &texture);
    };
}
#endif