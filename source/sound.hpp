#ifndef SOUND_HPP
#define SOUND_HPP

#include "vec3f.hpp"
#include "aiff.hpp"
#include "render.hpp"
#include "oggplayer/oggplayer.h"
#include <ogc/gu.h>
#include <cmath>
#include <asndlib.h>
#include <string>

class sound_t
{
private:
    aiff *aiff_data = nullptr;

public:
    vec3f position = vec3f(0., 0., 0.);
    float pitch = 1.0f;
    float volume = 1.0f;
    uint8_t left = 255;
    uint8_t right = 255;
    bool valid = false;
    int voice = -1;
    sound_t() {}
    sound_t(aiff_container &aiff_data);

    void set_aiff_data(aiff_container &aiff_data);
    void play();
    void pause();
    void stop();
    void update(vec3f head_right, vec3f head_position);
};

class sound_system_t
{
public:
    vec3f head_right = vec3f(1, 0, 0);
    vec3f head_position = vec3f(0, 0, 0);

    sound_t sounds[15];
    int frames_to_next_music = 0;

    sound_system_t();
    ~sound_system_t();

    void update(vec3f head_right, vec3f head_position);

    void play_sound(sound_t sound);

    void play_music(std::string filename);

};

#endif