#ifndef MODEL_HPP
#define MODEL_HPP

#include <vector>
#include <malloc.h>
#include <ogcsys.h>
#include <ogc/gx.h>
#include "vec3f.hpp"
#include "maths.hpp"

extern int tickCounter;

class modelbox_t
{
public:
    vec3f pos = vec3f(0, 0, 0);
    vec3f rot = vec3f(0, 0, 0);
    void *display_list = nullptr;
    uint32_t display_list_size = 0;
    modelbox_t(vec3f pivot = vec3f(0, 0, 0), vec3f size = vec3f(1, 1, 1), uint16_t uv_off_x = 0, uint16_t uv_off_y = 0, vfloat_t inflate = 0);
    modelbox_t(const modelbox_t &other) = delete;            // Prevent copying
    modelbox_t &operator=(const modelbox_t &other) = delete; // Prevent copying

    void render();

    void set_pos(const vec3f &pos)
    {
        this->pos = pos;
    }

    void set_rot(const vec3f &rot)
    {
        this->rot = rot;
    }

    ~modelbox_t()
    {
        if (display_list)
        {
            free(display_list);
        }
    }
};

class model_t
{
public:
    vec3f pos = vec3f(0, 0, 0);
    vec3f rot = vec3f(0, 0, 0);
    GXTexObj texture;
    std::vector<modelbox_t *> boxes;
    model_t() {}

    model_t(const model_t &other) = delete;            // Prevent copying
    model_t &operator=(const model_t &other) = delete; // Prevent copying

    virtual void render(float partial_ticks);

    modelbox_t *add_box(vec3f pos, vec3f size, uint16_t uv_off_x, uint16_t uv_off_y, vfloat_t inflate)
    {
        boxes.push_back(new modelbox_t(pos, size, uv_off_x, uv_off_y, inflate));
        return boxes.back();
    }

    ~model_t()
    {
        for (auto &box : boxes)
        {
            delete box;
        }
        boxes.clear();
    }
};

class creeper_model_t : public model_t
{
public:
    modelbox_t *head = nullptr;
    modelbox_t *body = nullptr;
    modelbox_t *leg1 = nullptr;
    modelbox_t *leg2 = nullptr;
    modelbox_t *leg3 = nullptr;
    modelbox_t *leg4 = nullptr;
    float speed = 0.0f;
    vec3f head_rot = vec3f(0, 0, 0);
    creeper_model_t()
    {
        head = add_box(vec3f(-4, -8, -4), vec3f(8, 8, 8), 0, 0, 0);
        head->set_pos(vec3f(0, 4, 0));
        body = add_box(vec3f(-4, 0, -2), vec3f(8, 12, 4), 16, 16, 0);
        body->set_pos(vec3f(0, 4, 0));
        leg1 = add_box(vec3f(-2, 0, -2), vec3f(4, 6, 4), 0, 16, 0);
        leg1->set_pos(vec3f(-2, 16, 4));
        leg2 = add_box(vec3f(-2, 0, -2), vec3f(4, 6, 4), 0, 16, 0);
        leg2->set_pos(vec3f(2, 16, 4));
        leg3 = add_box(vec3f(-2, 0, -2), vec3f(4, 6, 4), 0, 16, 0);
        leg3->set_pos(vec3f(-2, 16, -4));
        leg4 = add_box(vec3f(-2, 0, -2), vec3f(4, 6, 4), 0, 16, 0);
        leg4->set_pos(vec3f(2, 16, -4));
    }

    void render(float partial_ticks) override
    {
        float ticks = tickCounter + partial_ticks;

        head->set_rot(head_rot - rot);

        float leg_rot = std::cos(ticks * 0.3331f) * 28 * speed;
        float leg_rot2 = std::cos(ticks * 0.3331f + 3.1415927f) * 28 * speed;
        leg1->set_rot(vec3f(leg_rot, 0, 0));
        leg2->set_rot(vec3f(leg_rot2, 0, 0));
        leg3->set_rot(vec3f(leg_rot2, 0, 0));
        leg4->set_rot(vec3f(leg_rot, 0, 0));
        model_t::render(ticks);
    }
};

#endif