#ifndef SOUND_HPP
#define SOUND_HPP

#include <math/vec3f.hpp>
#include <ogc/gu.h>
#include <cmath>
#include <asndlib.h>
#include <string>
#include <auxio/aiff.hpp>
#include <auxio/oggplayer.h>

class Sound
{
private:
    Aiff *aiff_data = nullptr;

public:
    Vec3f position = Vec3f(0., 0., 0.);
    float pitch = 1.0f;
    float volume = 1.0f;
    uint8_t left = 255;
    uint8_t right = 255;
    bool valid = false;
    int voice = -1;
    Sound() {}
    Sound(AiffContainer &aiff_data);
    Sound(AiffContainer* aiff_data);

    void set_aiff_data(AiffContainer &aiff_data);
    void play();
    void pause();
    void stop();
    void update(Vec3f head_right, Vec3f head_position);
};

class SoundSystem
{
public:
    Vec3f head_right = Vec3f(1, 0, 0);
    Vec3f head_position = Vec3f(0, 0, 0);

    Sound sounds[15];
    int frames_to_next_music = 0;

    SoundSystem();
    ~SoundSystem();

    void update(Vec3f head_right, Vec3f head_position);

    void play_sound(Sound sound);

    void play_music(std::string filename);

};

#endif