#include "sound.hpp"
#include "timers.hpp"
#include "sounds.hpp"
sound_t::sound_t(aiff_container &aiff_data)
{
    set_aiff_data(aiff_data);
}

void sound_t::set_aiff_data(aiff_container &aiff_data)
{
    this->valid = aiff_data.data != nullptr;
    if (!valid)
        return;
    this->aiff_data = aiff_data.data;
}

void sound_t::play()
{
    if (!valid)
        return;
    if (voice > 0)
        return;
    int first_unused = ASND_GetFirstUnusedVoice();
    if (first_unused < 1)
        return;
    voice = first_unused;
    ASND_SetVoice(first_unused, VOICE_MONO_16BIT, 16000 * pitch, 0, aiff_data->sound_data.sound_data, aiff_data->sound_data.chunk_size, 255, 255, nullptr);
    ASND_PauseVoice(first_unused, 0);
}

void sound_t::pause()
{
    if (!valid)
        return;
    if (voice < 1)
        return;
}

void sound_t::stop()
{
    if (!valid)
        return;
    if (voice < 1)
        return;
}

void sound_t::update(vec3f head_right, vec3f head_position)
{
    if (!valid)
        return;
    if (voice < 1)
        return;
    if (ASND_StatusVoice(voice) == SND_UNUSED)
    {
        voice = -1;
        return;
    }

    vec3f relative_position = position - head_position;

    guVector head_direction = head_right;
    guVector sound_direction = (position - head_position).normalize();

    // Calculate which side of the plane the sound is on
    float distance = guVecDotProduct(&head_direction, &sound_direction);

    float attenuation = 16.f;

    // Calculate overall volume based on the distance to the sound
    float head_distance = (attenuation - relative_position.magnitude()) / attenuation;

    // Clamp the distance to the range [0, 1]
    float clamped_distance = std::max(0.0f, std::min(1.0f, head_distance)) * volume;

    // Used for smoothing the panning value
    static int previous_pan = 0;

    // Calculate the volume based on the distance to the plane
    int pan = distance * 192;

    // Clamp the panning value to the range [-255, 255]
    pan = std::max(-255, std::min(255, pan));

    // Smooth the panning value given a threshold
    if (std::abs(pan - previous_pan) > 10)
    {
        // Apply linear interpolation to smooth the panning value
        float lerp_factor = 0.1f;
        pan = previous_pan + (pan - previous_pan) * lerp_factor;
    }

    // Update the previous panning value
    previous_pan = pan;

    // Calculate left and right volume based on the panning value
    int right_volume = 255 - std::max(0, pan);
    int left_volume = 255 + std::min(0, pan);

    ASND_ChangeVolumeVoice(voice, left_volume * clamped_distance, right_volume * clamped_distance);
}

sound_system_t::sound_system_t()
{
    ASND_Init();
    ASND_Pause(0);
}

sound_system_t::~sound_system_t()
{
    ASND_End();
}

void sound_system_t::update(vec3f head_right, vec3f head_position)
{
    for (int i = 0; i < 15; i++)
    {
        sounds[i].update(head_right, head_position);
    }
    if (StatusOgg() != OGG_STATUS_RUNNING)
    {
        frames_to_next_music--;
    }
    else
    {
        frames_to_next_music = (10 * 60 * 60) + (rand() % (10 * 60 * 60)); // 10 minutes to 20 minutes
    }

    if (frames_to_next_music <= 0)
    {
        play_music(get_random_music(music_files));
    }
}

void sound_system_t::play_sound(sound_t sound)
{
    if (!sound.valid)
        return;
    for (int i = 0; i < 15; i++)
    {
        sound_t &s = sounds[i];
        if (!s.valid || s.voice < 0)
        {
            s = sound;
            s.play();
            return;
        }
    }
}

void sound_system_t::play_music(std::string filename)
{
    // Ensure that the filename is not empty
    if (filename.empty())
        return;

    // Ensure that the file exists
    FILE *file = fopen(filename.c_str(), "rb");
    if (!file)
        return;
    fclose(file);

    // Check if the Ogg player is already running
    if (StatusOgg() == OGG_STATUS_RUNNING)
    {
        // Don't play the music if music is already playing
        return;
    }

    // Play the music file once
    PlayOggFile(filename.c_str(), 0, OGG_ONE_TIME);
}
