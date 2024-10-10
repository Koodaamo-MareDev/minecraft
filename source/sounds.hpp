#ifndef SOUNDS_HPP
#define SOUNDS_HPP

#include "aiff.hpp"
#include <map>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

extern std::vector<std::string> music_files;

aiff_container& get_random_sound(std::vector<aiff_container> &sounds);

std::string get_random_music(std::vector<std::string> &music_files);

aiff_container *get_sound(std::string name);

aiff_container* randomize_sound(std::string name, int count);
#endif