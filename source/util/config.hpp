#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <string>
#include <map>
#include <stdexcept>
#include <type_traits>

#include "debuglog.hpp"

struct configuration_value
{
private:
    std::string string_value = "";
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

    configuration_value &operator=(const std::string &value)
    {
        string_value = value;
        try
        {
            float_value = std::stof(value);
        }
        catch (const std::invalid_argument &)
        {
            try
            {
                float_value = std::stoi(value);
            }
            catch (const std::invalid_argument &)
            {
                debug::print("Invalid configuration value: %s\n", value.c_str());
                float_value = 0.0f; // Reset to 0 if conversion fails
            }
        }
        return *this;
    }

    configuration_value &operator=(float value)
    {
        float_value = value;
        string_value = std::to_string(value);
        return *this;
    }

    configuration_value &operator=(int value)
    {
        float_value = static_cast<float>(value);
        string_value = std::to_string(value);
        return *this;
    }
};

class configuration
{
private:
    std::map<std::string, configuration_value> kv_pairs;

    // Default config location
    // C++17 doesn't support constexpr for std::string
    constexpr static const char *default_config_location = "/apps/minecraft/config.txt";

public:
    configuration() {}

    configuration(std::string filename)
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
                    continue;
                try
                {
                    kv_pairs[key] = std::stof(value);
                }
                catch (const std::invalid_argument &)
                {
                    try
                    {
                        kv_pairs[key] = std::stoi(value);
                    }
                    catch (const std::invalid_argument &)
                    {
                        debug::print("Invalid configuration value for key '%s': %s\n", key.c_str(), value.c_str());
                        kv_pairs[key] = value; // If conversion fails, store as string
                    }
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
            file << pair.first << "=" << std::string(pair.second) << "\n";
        }
    }

    configuration_value &operator[](const std::string &key)
    {
        return kv_pairs.count(key) ? kv_pairs[key] : kv_pairs[key] = configuration_value();
    }

    template <typename T>
    configuration_value &get(const std::string &key, T default_value)
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