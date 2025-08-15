#ifndef SOUNDS_HPP
#define SOUNDS_HPP

#include <auxio/aiff.hpp>
#include <map>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

extern std::vector<std::string> music_files;

std::string get_random_music();

AiffContainer *get_sound(std::string name);

AiffContainer* randomize_sound(std::string name, int count);
#endif