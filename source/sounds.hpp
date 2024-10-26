#ifndef SOUNDS_HPP
#define SOUNDS_HPP

#include "aiff.hpp"
#include <map>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

extern std::vector<std::string> music_files;

std::string get_random_music();

aiff_container *get_sound(std::string name);

aiff_container* randomize_sound(std::string name, int count);
#endif