#include "sound.hpp"
#include "sounds.hpp"
#include <util/timers.hpp>
#include <util/constants.hpp>

Sound::Sound(AiffContainer *source)
{
    // If the container has no data, mark the sound as invalid. Otherwise, set the data pointer.
    if (!source || !source->data)
        return;
    this->aiff = source;
    this->valid = true;
}

void Sound::play()
{
    if (!valid)
        return;
    if (voice > 0)
        return;
    int first_unused = ASND_GetFirstUnusedVoice();
    if (first_unused < 1)
        return;
    voice = first_unused;
    ASND_SetVoice(voice, aiff->sample_format, aiff->sample_rate * pitch, 0, aiff->data, aiff->data_size, left, right, nullptr);
    ASND_PauseVoice(voice, 0);
}

void Sound::pause()
{
    if (!valid)
        return;
    if (voice < 1)
        return;
}

void Sound::stop()
{
    if (!valid)
        return;
    if (voice < 1)
        return;
}

void Sound::update(Vec3f head_right, Vec3f head_position)
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

    Vec3f relative_position = position - head_position;

    guVector head_direction = head_right;
    guVector sound_direction = (position - head_position).fast_normalize();

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

    this->left = left_volume * clamped_distance;
    this->right = right_volume * clamped_distance;

    ASND_ChangeVolumeVoice(voice, left, right);
}

SoundSystem::SoundSystem()
{
    ASND_Init();
    ASND_Pause(0);
}

SoundSystem::~SoundSystem()
{
    ASND_End();
}

void SoundSystem::update(Vec3f head_right, Vec3f head_position, bool enable_music)
{
    this->head_position = head_position;
    this->head_right = head_right;
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

    if (frames_to_next_music <= 0 && enable_music)
    {
        play_music(RESOURCES_DIR + get_random_music());
    }
}

void SoundSystem::play_sound(Sound snd)
{
    if (!snd.valid)
        return;
    for (int i = 0; i < 15; i++)
    {
        Sound &s = sounds[i];
        if (!s.valid || s.voice < 0)
        {
            s = snd;
            s.play();
            s.update(head_right, head_position);
            return;
        }
    }
}

void SoundSystem::play_music(std::string filename)
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
