#include "entity_player.hpp"
#include <world/world.hpp>
#include <sounds.hpp>
#include <ported/Random.hpp>
#include <registry/textures.hpp>

EntityPlayer::EntityPlayer(const Vec3f &position) : EntityLiving()
{
    this->width = 0.6f;
    this->height = 1.8;
    this->health = 20;
    this->walk_sound = true;
    this->gravity = 0.08;
    this->y_offset = 1.62;
    this->y_size = 0;
    this->drag_phase = DragPhase::before_friction;
    this->type = 48; // Mob - Not sure if this is correct as there is no player entity "type".
    teleport(position);
}

void EntityPlayer::hurt(int16_t damage)
{
    EntityLiving::hurt(damage);
    javaport::Random rng;
    Sound sound = get_sound("random/hurt");
    sound.position = world->player.position;
    sound.volume = 0.5;
    sound.pitch = rng.nextFloat() * 0.4 + 0.8;
    world->play_sound(sound);
}
