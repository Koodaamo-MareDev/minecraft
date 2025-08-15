#include "sounds.hpp"
#include <cstring>
#include "ported/Random.hpp"
#include "util/constants.hpp"

std::vector<std::string> music_files(
    {"music/calm1.ogg",
     "music/calm2.ogg",
     "music/calm3.ogg",
     "newmusic/hal1.ogg",
     "newmusic/hal2.ogg",
     "newmusic/hal3.ogg",
     "newmusic/hal4.ogg",
     "newmusic/nuance1.ogg",
     "newmusic/nuance2.ogg",
     "newmusic/piano1.ogg",
     "newmusic/piano2.ogg",
     "newmusic/piano3.ogg"});

std::string get_random_music()
{
    static javaport::Random random;
    if (music_files.empty())
    {
        return "";
    }

    return music_files[random.nextInt(music_files.size())];
}

std::map<std::string, AiffContainer *> sound_map;

AiffContainer *get_sound(std::string name)
{
    auto it = sound_map.find(name);
    if (it != sound_map.end())
    {
        return it->second;
    }
    std::string path = SOUND_DIR + name + ".Aiff";

    FILE *file = fopen(path.c_str(), "rb");
    if (!file)
    {
        printf("Failed to open sound file: %s\n", path.c_str());
        return nullptr;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *data = new (std::align_val_t(32)) uint8_t[size];
    if (!data)
    {
        fclose(file);
        return nullptr;
    }

    fread(data, size, 1, file);

    fclose(file);

    AiffContainer *sound = new AiffContainer(data);
    if (!sound->data)
    {
        // Aiff validation failed
        delete sound;
        delete[] data;
        return nullptr;
    }

    sound_map[name] = sound;
    return sound;
}

AiffContainer *randomize_sound(std::string name, int count)
{
    name += std::to_string((rand() % count) + 1);
    return get_sound(name);
}