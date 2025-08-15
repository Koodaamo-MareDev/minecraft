#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <string>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <cmath>

#include "debuglog.hpp"

struct ConfigurationValue
{
private:
    std::string string_value = "0.0";
    float float_value = 0.0f;

public:
    operator std::string() const
    {
        return string_value;
    }

    operator float() const
    {
        return float_value;
    }

    operator int() const
    {
        return static_cast<int>(float_value);
    }

    ConfigurationValue &operator=(const std::string &value)
    {
        string_value = value;
        if (value.empty())
        {
            float_value = 0.0f; // Reset to 0 if the string is empty
            return *this;
        }
        // Only permit up to one dot in the string (lazy way of determining if it's an IP address or a float)
        size_t dot_pos = std::string(value).find('.');
        size_t last_dot = std::string(value).rfind('.');

        // Check if the value is a number
        if (std::string(value).find_first_not_of("0123456789.-") == std::string::npos && (dot_pos == std::string::npos || dot_pos == last_dot))
        {
            // If the string is numeric, convert to float
            try
            {
                float_value = std::stof(value);
            }
            catch (const std::invalid_argument &)
            {
                float_value = 0.0f; // Reset to 0 if conversion fails
            }
        }
        return *this;
    }

    ConfigurationValue &operator=(float value)
    {
        float_value = value;
        string_value = std::to_string(value);
        return *this;
    }

    ConfigurationValue &operator=(int value)
    {
        float_value = static_cast<float>(value);
        string_value = std::to_string(value);
        return *this;
    }
};

class Configuration
{
private:
    std::map<std::string, ConfigurationValue> kv_pairs;

    // Default config location
    // C++17 doesn't support constexpr for std::string
    constexpr static const char *default_config_location = "/apps/minecraft/config.txt";

public:
    Configuration() {}

    Configuration(std::string filename)
    {
        load(filename);
    }

    void load(std::string filename = default_config_location)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("Could not load config");

        std::string line;
        while (std::getline(file, line))
        {
            size_t pos = line.find('=');
            if (pos != std::string::npos)
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                if (value.empty())
                {
                    kv_pairs[key] = value;
                    continue; // Skip empty values
                }
                // Only permit up to one dot in the string (lazy way of determining if it's an IP address or a float)
                size_t dot_pos = std::string(value).find('.');
                size_t last_dot = std::string(value).rfind('.');

                // Check if the value is a number
                if (std::string(value).find_first_not_of("0123456789.-") == std::string::npos && (dot_pos == std::string::npos || dot_pos == last_dot))
                {
                    try
                    {
                        kv_pairs[key] = std::stof(value);
                    }
                    catch (const std::invalid_argument &)
                    {
                        kv_pairs[key] = 0.0f; // Reset to 0 if conversion fails
                    }
                }
                else
                {
                    kv_pairs[key] = value;
                }
            }
        }
    }

    void save(std::string filename = default_config_location)
    {
        std::ofstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("Could not save config");

        for (const auto &pair : kv_pairs)
        {
            ConfigurationValue value = pair.second;

            if (std::string(value).empty())
            {
                file << pair.first << "=\n"; // Save empty values as empty
                continue;
            }

            // Only permit up to one dot in the string (lazy way of determining if it's an IP address or a float)
            size_t dot_pos = std::string(value).find('.');
            size_t last_dot = std::string(value).rfind('.');

            // Check if the value is a number
            if (std::string(value).find_first_not_of("0123456789.-") == std::string::npos && (dot_pos == std::string::npos || dot_pos == last_dot))
            {
                // Check if the value is effectively an integer
                if (std::floor(float(value)) == float(value))
                    file << pair.first << "=" << int(value) << "\n";
                else // Otherwise, save as float
                    file << pair.first << "=" << float(value) << "\n";
            }
            else
            {
                // Save as string
                file << pair.first << "=" << std::string(value) << "\n";
            }
        }
    }

    ConfigurationValue &operator[](const std::string &key)
    {
        return kv_pairs.count(key) ? kv_pairs[key] : kv_pairs[key] = ConfigurationValue();
    }

    template <typename T>
    ConfigurationValue &get(const std::string &key, T default_value)
    {
        static_assert(std::is_same<T, std::string>::value || std::is_same<T, float>::value || std::is_same<T, int>::value, "Type must be string, float or int");

        if (!kv_pairs.count(key))
        {
            kv_pairs[key] = default_value;
        }

        return kv_pairs[key];
    }
};

#endif