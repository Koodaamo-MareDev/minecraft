#ifndef SOUND_HPP
#define SOUND_HPP

#include "vec3f.hpp"
#include "aiff.hpp"
#include "render.hpp"
#include <ogc/gu.h>
#include <cmath>
#include <asndlib.h>

class sound
{
public:
    vec3f position = vec3f(0., 0., 0.);
    float pitch = 1.0f;
    bool valid = false;
    int voice = -1;
    aiff *aiff_data = nullptr;

    sound(aiff *aiff_data);

    void play();
    void pause();
    void stop();
    void update(vec3f head_right, vec3f head_position);

};



#endif