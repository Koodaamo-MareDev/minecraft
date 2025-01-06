#ifndef UTF2ASCII_HPP
#define UTF2ASCII_HPP

#include <string>
#include <cstdint>

// Convert a UTF-8 string to extended ASCII - this is a potentially lossy conversion
static std::string utf8ToExtendedASCII(const std::string &utf8Str)
{
    std::string extendedASCII;

    for (size_t i = 0; i < utf8Str.size();)
    {
        unsigned char c = utf8Str[i];

        if (c < 128)
        {
            // ASCII range (single byte)
            extendedASCII.push_back(c);
            i++;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // Two-byte UTF-8 sequence
            if (i + 1 < utf8Str.size() && (utf8Str[i + 1] & 0xC0) == 0x80)
            {
                uint16_t codePoint = ((c & 0x1F) << 6) | (utf8Str[i + 1] & 0x3F);
                if (codePoint >= 128 && codePoint <= 255)
                {
                    // Extended ASCII range
                    extendedASCII.push_back(static_cast<char>(codePoint));
                }
                else
                {
                    extendedASCII.push_back(0xB0); // Replacement for out-of-range code points
                }
                i += 2;
            }
            else
            {
                extendedASCII.push_back(0xB0); // Invalid sequence
                i++;
            }
        }
        else
        {
            // All other cases: invalid or unsupported multi-byte sequences
            extendedASCII.push_back(0xB0);
            i++;
        }
    }

    return extendedASCII;
}

#endif // UTF2ASCII_HPP