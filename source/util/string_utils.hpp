#ifndef STRING_UTILS_HPP
#define STRING_UTILS_HPP

#include <string>
#include <sstream>
#include <vector>
#include <codecvt>
#include <locale>
#include <unordered_map>

namespace str
{

    inline std::vector<std::string> split(const std::string &str, char delimiter)
    {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, delimiter))
        {
            tokens.push_back(token);
        }

        return tokens;
    }

    inline std::string ftos(float value, int precision = 2)
    {
        std::ostringstream out;
        out.precision(precision);
        out << std::fixed << value;
        return out.str();
    }

    inline std::string utf8_to_cp437(const std::string &utf8_str)
    {
        // Unicode to CP437 mapping for extended characters
        static const std::unordered_map<char32_t, unsigned char> unicode_to_cp437 = {
            {U'Ç', 0x80}, {U'ü', 0x81}, {U'é', 0x82}, {U'â', 0x83}, {U'ä', 0x84}, {U'à', 0x85}, {U'å', 0x86}, {U'ç', 0x87},
            {U'ê', 0x88}, {U'ë', 0x89}, {U'è', 0x8A}, {U'ï', 0x8B}, {U'î', 0x8C}, {U'ì', 0x8D}, {U'Ä', 0x8E}, {U'Å', 0x8F},
            {U'É', 0x90}, {U'æ', 0x91}, {U'Æ', 0x92}, {U'ô', 0x93}, {U'ö', 0x94}, {U'ò', 0x95}, {U'û', 0x96}, {U'ù', 0x97},
            {U'ÿ', 0x98}, {U'Ö', 0x99}, {U'Ü', 0x9A}, {U'¢', 0x9B}, {U'£', 0x9C}, {U'¥', 0x9D}, {U'₧', 0x9E}, {U'ƒ', 0x9F},
            {U'á', 0xA0}, {U'í', 0xA1}, {U'ó', 0xA2}, {U'ú', 0xA3}, {U'ñ', 0xA4}, {U'Ñ', 0xA5}, {U'ª', 0xA6}, {U'§', 0xA7},
            {U'¿', 0xA8}, {U'⌐', 0xA9}, {U'¬', 0xAA}, {U'½', 0xAB}, {U'¼', 0xAC}, {U'¡', 0xAD}, {U'«', 0xAE}, {U'»', 0xAF},
            {U'░', 0xB0}, {U'▒', 0xB1}, {U'▓', 0xB2}, {U'│', 0xB3}, {U'┤', 0xB4}, {U'╡', 0xB5}, {U'╢', 0xB6}, {U'╖', 0xB7},
            {U'╕', 0xB8}, {U'╣', 0xB9}, {U'║', 0xBA}, {U'╗', 0xBB}, {U'╝', 0xBC}, {U'╜', 0xBD}, {U'╛', 0xBE}, {U'┐', 0xBF},
            {U'└', 0xC0}, {U'┴', 0xC1}, {U'┬', 0xC2}, {U'├', 0xC3}, {U'─', 0xC4}, {U'┼', 0xC5}, {U'╞', 0xC6}, {U'╟', 0xC7},
            {U'╚', 0xC8}, {U'╔', 0xC9}, {U'╩', 0xCA}, {U'╦', 0xCB}, {U'╠', 0xCC}, {U'═', 0xCD}, {U'╬', 0xCE}, {U'╧', 0xCF},
            {U'╨', 0xD0}, {U'╤', 0xD1}, {U'╥', 0xD2}, {U'╙', 0xD3}, {U'╘', 0xD4}, {U'╒', 0xD5}, {U'╓', 0xD6}, {U'╫', 0xD7},
            {U'╪', 0xD8}, {U'┘', 0xD9}, {U'┌', 0xDA}, {U'█', 0xDB}, {U'▄', 0xDC}, {U'▌', 0xDD}, {U'▐', 0xDE}, {U'▀', 0xDF},
            {U'α', 0xE0}, {U'ß', 0xE1}, {U'Γ', 0xE2}, {U'π', 0xE3}, {U'Σ', 0xE4}, {U'σ', 0xE5}, {U'µ', 0xE6}, {U'τ', 0xE7},
            {U'Φ', 0xE8}, {U'Θ', 0xE9}, {U'Ω', 0xEA}, {U'δ', 0xEB}, {U'∞', 0xEC}, {U'φ', 0xED}, {U'ε', 0xEE}, {U'∩', 0xEF},
            {U'≡', 0xF0}, {U'±', 0xF1}, {U'≥', 0xF2}, {U'≤', 0xF3}, {U'⌠', 0xF4}, {U'⌡', 0xF5}, {U'÷', 0xF6}, {U'≈', 0xF7},
            {U'°', 0xF8}, {U'∙', 0xF9}, {U'·', 0xFA}, {U'√', 0xFB}, {U'ⁿ', 0xFC}, {U'²', 0xFD}, {U'■', 0xFE}, {U' ', 0xFF}
        };

        auto is_extended = [](char c)
        {
            return (c & 0x80) != 0;
        };

        if (std::find_if(utf8_str.begin(), utf8_str.end(), is_extended) == utf8_str.end())
        {
            // String contains only standard ASCII characters
            return utf8_str;
        }

        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
        std::u32string u32_str;
        try
        {
            u32_str = convert.from_bytes(utf8_str);
        }
        catch (const std::range_error &e)
        {
            return utf8_str; // Fallback to original string on error
        }
        std::string eascii_str;

        for (char32_t ch : u32_str)
        {
            if (ch <= 0x7F)
            {
                // Standard ASCII - pass through unchanged
                eascii_str += static_cast<char>(ch);
            }
            else
            {
                // Extended character - look up in mapping table
                auto it = unicode_to_cp437.find(ch);
                if (it != unicode_to_cp437.end())
                {
                    eascii_str += static_cast<char>(it->second);
                }
                else
                {
                    // Character not found in CP437, use '?' as fallback
                    eascii_str += '?';
                }
            }
        }
        return eascii_str;
    }

}
#endif