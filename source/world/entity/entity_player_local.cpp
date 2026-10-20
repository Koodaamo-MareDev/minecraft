#include "entity_player_local.hpp"
#include <world/world.hpp>
#include <render/render.hpp>
#include <render/model.hpp>
#include <util/input/input.hpp>
#include <gui/gui.hpp>

EntityPlayerLocal::EntityPlayerLocal(const Vec3f &position) : EntityPlayer(position)
{
    simulate_offline = true;
    teleport(position);
}

void EntityPlayerLocal::serialize(NBTTagCompound *result)
{
    EntityLiving::serialize(result);

    result->setTag("Inventory", new NBTTagList);
    result->setTag("Dimension", new NBTTagInt(-int(world->hell)));
    result->setTag("Score", new NBTTagInt(0));
    result->setTag("Sleeping", new NBTTagByte(in_bed));
    result->setTag("SleepTimer", new NBTTagShort(0));
    result->setTag("Inventory", this->items.serialize());
}

void EntityPlayerLocal::deserialize(NBTTagCompound *result)
{
    EntityLiving::deserialize(result);

    world->set_hell(result->getInt("Dimension") == -1);
    in_bed = result->getByte("Sleeping");
    NBTTagList *inv = dynamic_cast<NBTTagList *>(result->getTag("Inventory"));
    if (inv)
        this->items.deserialize(inv);
}

void EntityPlayerLocal::hurt(int16_t damage)
{
    EntityPlayer::hurt(damage);
    health_update_tick = 10;
    guVector rot = rotation;
    rot.z += 8;
    get_camera().transform.set_rotation(rot); // Tilt the camera a bit
}

void EntityPlayerLocal::tick()
{
    if (health_update_tick > 0)
        health_update_tick--;
    if (!Gui::get())
    {
        Camera &camera = get_camera();
        for (input::Device *dev : input::devices)
        {
            if (dev->connected())
            {
                // Update the cursor position based on the left stick
                Vec3f left_stick = dev->get_left_stick();
                float y_rot = camera.transform.get_rotation().y;
                movement.x = left_stick.x * sin(DegToRad(y_rot + 90));
                movement.z = left_stick.x * cos(DegToRad(y_rot + 90));
                movement.x -= left_stick.y * sin(DegToRad(y_rot));
                movement.z -= left_stick.y * cos(DegToRad(y_rot));
                movement.y = 0;
                if (movement.magnitude() > 1)
                {
                    movement = movement.fast_normalize();
                }
            }
        }
    }
    else
    {
        // If a GUI is open, reset the movement
        movement = Vec3f(0);
    }
    EntityLiving::tick();
    if (!world->is_remote() && aabb.min.y < -750)
        teleport(Vec3f(position.x, 256, position.z));
}

void EntityPlayerLocal::render(float partial_ticks, bool transparency)
{
    // For now, we're not going to render the local player
}

void EntityPlayerLocal::animate()
{
    // Stub as we don't want to animate the local player
}

bool EntityPlayerLocal::should_jump()
{
    if (Gui::get())
        return false;

    for (input::Device *dev : input::devices)
    {
        if (dev->connected() && (dev->get_buttons_held() & input::BUTTON_JUMP))
        {
            return true;
        }
    }
    return false;
}

bool EntityPlayerLocal::can_remove()
{
    return false;
}
