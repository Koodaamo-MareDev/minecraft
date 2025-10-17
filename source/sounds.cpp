#include "sounds.hpp"
#include <cstring>
#include <fstream>
#include "ported/Random.hpp"
#include "util/constants.hpp"
#include "util/debuglog.hpp"

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
    std::string path = SOUND_DIR + name + ".aiff";

    // Open the file and get its size
    std::ifstream file(path, std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    if (size < sizeof(Aiff))
    {
        // Could be a corrupted file or too small to be a valid AIFF
        debug::print("Failed to load sound %s: File is too small\n", path.c_str());
        return nullptr;
    }

    // Allocate temporary buffer for the file data
    uint8_t *data = new (std::nothrow) uint8_t[size];
    if (!data)
    {
        debug::print("Failed to allocate memory for sound %s\n", path.c_str());
        return nullptr;
    }

    // Seek back to the beginning of the file and read the data
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(data), size);
    file.close();

    try
    {
        AiffContainer *sound = new AiffContainer(data);
        sound_map[name] = sound;

        // The aiff container will copy the data it needs, so we'll free the original buffer after creating it.
        delete[] data;
        return sound;
    }
    catch (std::runtime_error &e)
    {
        debug::print("Failed to load sound %s: %s\n", path.c_str(), e.what());
        // Aiff validation failed
        delete[] data;
        return nullptr;
    }
}

AiffContainer *randomize_sound(std::string name, int count)
{
    static javaport::Random random;
    if (count < 1)
        return nullptr;
    name += std::to_string((random.nextInt(count)) + 1);
    return get_sound(name);
}