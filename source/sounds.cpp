#include "sounds.hpp"

std::vector<aiff_container> pling_sounds({aiff_container(pling_aiff)});

std::vector<aiff_container> cloth_sounds({aiff_container(cloth1_aiff),
                                          aiff_container(cloth2_aiff),
                                          aiff_container(cloth3_aiff),
                                          aiff_container(cloth4_aiff)});

std::vector<aiff_container> glass_sounds({aiff_container(glass1_aiff),
                                          aiff_container(glass2_aiff),
                                          aiff_container(glass3_aiff)});

std::vector<aiff_container> grass_sounds({aiff_container(grass1_aiff),
                                          aiff_container(grass2_aiff),
                                          aiff_container(grass3_aiff),
                                          aiff_container(grass4_aiff)});

std::vector<aiff_container> gravel_sounds({aiff_container(gravel1_aiff),
                                           aiff_container(gravel2_aiff),
                                           aiff_container(gravel3_aiff),
                                           aiff_container(gravel4_aiff)});

std::vector<aiff_container> sand_sounds({aiff_container(sand1_aiff),
                                         aiff_container(sand2_aiff),
                                         aiff_container(sand3_aiff),
                                         aiff_container(sand4_aiff)});

std::vector<aiff_container> wood_sounds({aiff_container(wood1_aiff),
                                         aiff_container(wood2_aiff),
                                         aiff_container(wood3_aiff),
                                         aiff_container(wood4_aiff)});

std::vector<aiff_container> stone_sounds({aiff_container(stone1_aiff),
                                          aiff_container(stone2_aiff),
                                          aiff_container(stone3_aiff),
                                          aiff_container(stone4_aiff)});

aiff_container explode_sound(explode_aiff);

aiff_container fuse_sound(fuse_aiff);

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
