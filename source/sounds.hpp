#ifndef SOUNDS_HPP
#define SOUNDS_HPP

#include "aiff.hpp"
#include <vector>
#include <string>
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

#include "explode_aiff.h"

extern std::vector<aiff_container> pling_sounds;
extern std::vector<aiff_container> cloth_sounds;
extern std::vector<aiff_container> glass_sounds;
extern std::vector<aiff_container> grass_sounds;
extern std::vector<aiff_container> gravel_sounds;
extern std::vector<aiff_container> sand_sounds;
extern std::vector<aiff_container> wood_sounds;
extern std::vector<aiff_container> stone_sounds;

extern std::vector<std::string> music_files;

extern aiff_container explode_sound;

aiff_container& get_random_sound(std::vector<aiff_container> &sounds);

std::string get_random_music(std::vector<std::string> &music_files);

#endif