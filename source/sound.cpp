#include "sound.hpp"

sound::sound(aiff *aiff_data)
{
    valid = aiff_data != nullptr;
    if (!valid)
        return;
    this->aiff_data = aiff_data;
}

void sound::play()
{
    if (!valid)
        return;
    if (voice > 0)
        return;
    int first_unused = ASND_GetFirstUnusedVoice();
    if (first_unused < 1)
        return;
    voice = first_unused;
    ASND_SetVoice(first_unused, VOICE_MONO_16BIT, 16000, 0, aiff_data->sound_data.sound_data, aiff_data->sound_data.chunk_size, 255, 255, nullptr);
    ASND_Pause(0);
    ASND_PauseVoice(first_unused, 0);
}

void sound::pause()
{
    if (!valid)
        return;
    if (voice < 1)
        return;
}

void sound::stop()
{
    if (!valid)
        return;
    if (voice < 1)
        return;
}

void sound::update(vec3f head_right, vec3f head_position)
{
    if (!valid)
        return;
    if (voice < 1)
        return;
    if(ASND_StatusVoice(voice) == SND_UNUSED)
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
    float clamped_distance = std::max(0.0f, std::min(1.0f, head_distance));

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