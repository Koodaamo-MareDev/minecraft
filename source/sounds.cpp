#include "sounds.hpp"
#include <cstring>

aiff_container &get_random_sound(std::vector<aiff_container> &sounds)
{
    return sounds[rand() % sounds.size()];
}

std::vector<std::string> music_files(
    {"calm1.ogg",
     "calm2.ogg",
     "calm3.ogg",
     "hal1.ogg",
     "hal2.ogg",
     "hal3.ogg",
     "hal4.ogg",
     "nuance1.ogg",
     "nuance2.ogg",
     "piano1.ogg",
     "piano2.ogg",
     "piano3.ogg"});

std::string get_random_music(std::vector<std::string> &music_files)
{
    if (music_files.empty())
    {
        return "";
    }

    return music_files[rand() % music_files.size()];
}

std::map<std::string, aiff_container*> sound_map;

aiff_container *get_sound(std::string name)
{
    auto it = sound_map.find(name);
    if (it != sound_map.end())
    {
        return it->second;
    }
    std::string path = "sound/" + name + ".aiff";

    FILE *file = fopen(path.c_str(), "rb");
    if (!file)
    {
        printf("Failed to open sound file: %s\n", path.c_str());
        return nullptr;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t* data = new(std::align_val_t(32)) uint8_t[size];
    if (!data)
    {
        fclose(file);
        return nullptr;
    }

    fread(data, size, 1, file);

    fclose(file);

    aiff_container *sound = new aiff_container(data);
    if(!sound->data)
    {
        // Aiff validation failed
        delete sound;
        delete[] data;
        return nullptr;
    }
    
    sound_map[name] = sound;
    return sound;
}

aiff_container* randomize_sound(std::string name, int count)
{
    name += std::to_string((rand() % count) + 1);
    return get_sound(name);
}