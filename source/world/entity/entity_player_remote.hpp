#pragma once
#include <world/entity/entity_player.hpp>

class EntityPlayerMp : public EntityPlayer
{
public:
    EntityPlayerMp(const Vec3f &position, std::string player_name);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};