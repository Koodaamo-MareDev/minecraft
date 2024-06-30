#ifndef SOUNDS_HPP
#define SOUNDS_HPP

#include "aiff.hpp"
#include <vector>
#include <cstdlib>

#include "cloth1_aiff.h"
#include "cloth2_aiff.h"
#include "cloth3_aiff.h"
#include "cloth4_aiff.h"

#include "glass1_aiff.h"
#include "glass2_aiff.h"
#include "glass3_aiff.h"

#include "pling_aiff.h"
#include "grass1_aiff.h"
#include "grass2_aiff.h"
#include "grass3_aiff.h"
#include "grass4_aiff.h"

#include "gravel1_aiff.h"
#include "gravel2_aiff.h"
#include "gravel3_aiff.h"
#include "gravel4_aiff.h"

#include "sand1_aiff.h"
#include "sand2_aiff.h"
#include "sand3_aiff.h"
#include "sand4_aiff.h"

#include "wood1_aiff.h"
#include "wood2_aiff.h"
#include "wood3_aiff.h"
#include "wood4_aiff.h"

#include "stone1_aiff.h"
#include "stone2_aiff.h"
#include "stone3_aiff.h"
#include "stone4_aiff.h"

std::vector<aiff_container> pling_sounds({
    aiff_container(pling_aiff)
});

std::vector<aiff_container> cloth_sounds({
    aiff_container(cloth1_aiff),
    aiff_container(cloth2_aiff),
    aiff_container(cloth3_aiff),
    aiff_container(cloth4_aiff)
});

std::vector<aiff_container> glass_sounds({
    aiff_container(glass1_aiff),
    aiff_container(glass2_aiff),
    aiff_container(glass3_aiff)
});

std::vector<aiff_container> grass_sounds({
    aiff_container(grass1_aiff),
    aiff_container(grass2_aiff),
    aiff_container(grass3_aiff),
    aiff_container(grass4_aiff)
});

std::vector<aiff_container> gravel_sounds({
    aiff_container(gravel1_aiff),
    aiff_container(gravel2_aiff),
    aiff_container(gravel3_aiff),
    aiff_container(gravel4_aiff)
});

std::vector<aiff_container> sand_sounds({
    aiff_container(sand1_aiff),
    aiff_container(sand2_aiff),
    aiff_container(sand3_aiff),
    aiff_container(sand4_aiff)
});

std::vector<aiff_container> wood_sounds({
    aiff_container(wood1_aiff),
    aiff_container(wood2_aiff),
    aiff_container(wood3_aiff),
    aiff_container(wood4_aiff)
});

std::vector<aiff_container> stone_sounds({
    aiff_container(stone1_aiff),
    aiff_container(stone2_aiff),
    aiff_container(stone3_aiff),
    aiff_container(stone4_aiff)
});

inline aiff_container& get_random_sound(std::vector<aiff_container> &sounds)
{
    return sounds[rand() % sounds.size()];
}

#endif