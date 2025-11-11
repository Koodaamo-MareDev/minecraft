#ifndef MODEL_HPP
#define MODEL_HPP

#include <vector>
#include <ogcsys.h>
#include <ogc/gx.h>
#include <math/vec3f.hpp>
#include <math/math_utils.h>
#include <item/inventory.hpp>
#include <gertex/gertex.hpp>

class ModelBox
{
public:
    Vec3f pos = Vec3f(0, 0, 0);
    Vec3f rot = Vec3f(0, 0, 0);
    uint8_t *display_list = nullptr;
    uint32_t display_list_size = 0;
    bool visible = true;
    ModelBox(Vec3f pivot = Vec3f(0, 0, 0), Vec3f size = Vec3f(1, 1, 1), uint16_t uv_off_x = 0, uint16_t uv_off_y = 0, vfloat_t inflate = 0) : pivot(pivot), size(size), uv_off_x(uv_off_x), uv_off_y(uv_off_y), inflate(inflate) {}
    ModelBox(const ModelBox &other) = delete;            // Prevent copying
    ModelBox &operator=(const ModelBox &other) = delete; // Prevent copying

    void prepare();

    void render();

    void set_pos(const Vec3f &pos)
    {
        this->pos = pos;
    }

    void set_rot(const Vec3f &rot)
    {
        this->rot = rot;
    }

    ~ModelBox()
    {
        if (display_list)
        {
            delete[] display_list;
        }
    }

private:
    Vec3f pivot;
    Vec3f size;
    uint16_t uv_off_x;
    uint16_t uv_off_y;
    vfloat_t inflate;
};

class Model
{
public:
    Vec3f pos = Vec3f(0, 0, 0);
    Vec3f rot = Vec3f(0, 0, 0);
    GXTexObj texture;
    std::vector<ModelBox *> boxes;
    bool ready = false;
    Model() {}

    Model(const Model &other) = delete;            // Prevent copying
    Model &operator=(const Model &other) = delete; // Prevent copying

    void prepare();

    virtual void render(vfloat_t distance, float partial_ticks, bool transparency);
    void render_handitem(ModelBox *box, inventory::ItemStack &item, Vec3f pos, Vec3f rot, Vec3f scale, bool transparency);

    ModelBox *add_box(Vec3f pos, Vec3f size, uint16_t uv_off_x, uint16_t uv_off_y, vfloat_t inflate)
    {
        boxes.push_back(new ModelBox(pos, size, uv_off_x, uv_off_y, inflate));
        return boxes.back();
    }

    ~Model()
    {
        for (auto &box : boxes)
        {
            delete box;
        }
        boxes.clear();
    }
};

class CreeperModel : public Model
{
public:
    ModelBox *head = nullptr;
    ModelBox *body = nullptr;
    ModelBox *leg1 = nullptr;
    ModelBox *leg2 = nullptr;
    ModelBox *leg3 = nullptr;
    ModelBox *leg4 = nullptr;
    float speed = 0.0f;
    Vec3f head_rot = Vec3f(0, 0, 0);
    CreeperModel()
    {
        head = add_box(Vec3f(-4, -8, -4), Vec3f(8, 8, 8), 0, 0, 0);
        head->set_pos(Vec3f(0, 4, 0));
        body = add_box(Vec3f(-4, 0, -2), Vec3f(8, 12, 4), 16, 16, 0);
        body->set_pos(Vec3f(0, 4, 0));
        leg1 = add_box(Vec3f(-2, 0, -2), Vec3f(4, 6, 4), 0, 16, 0);
        leg1->set_pos(Vec3f(-2, 16, 4));
        leg2 = add_box(Vec3f(-2, 0, -2), Vec3f(4, 6, 4), 0, 16, 0);
        leg2->set_pos(Vec3f(2, 16, 4));
        leg3 = add_box(Vec3f(-2, 0, -2), Vec3f(4, 6, 4), 0, 16, 0);
        leg3->set_pos(Vec3f(-2, 16, -4));
        leg4 = add_box(Vec3f(-2, 0, -2), Vec3f(4, 6, 4), 0, 16, 0);
        leg4->set_pos(Vec3f(2, 16, -4));
    }

    void render(vfloat_t distance, float partial_ticks, bool transparency) override
    {
        head->set_rot(head_rot - rot);
        float leg_rot = std::cos(distance * 0.6662f) * 1.4f * speed / M_DTOR;
        float leg_rot2 = std::cos(distance * 0.6662f + M_PI) * 1.4f * speed / M_DTOR;
        leg1->set_rot(Vec3f(leg_rot, 0, 0));
        leg2->set_rot(Vec3f(leg_rot2, 0, 0));
        leg3->set_rot(Vec3f(leg_rot2, 0, 0));
        leg4->set_rot(Vec3f(leg_rot, 0, 0));
        Model::render(distance, partial_ticks, transparency);
    }
};

class PlayerModel : public Model
{
public:
    ModelBox *head = nullptr;
    ModelBox *body = nullptr;
    ModelBox *left_arm = nullptr;
    ModelBox *right_arm = nullptr;
    ModelBox *left_leg = nullptr;
    ModelBox *right_leg = nullptr;
    inventory::ItemStack equipment[5] = {}; // 0: hand, 1: helmet, 2: chestplate, 3: leggings, 4: boots
    float speed = 0.0f;
    Vec3f head_rot = Vec3f(0, 0, 0);
    PlayerModel()
    {
        head = add_box(Vec3f(-4, -8, -4), Vec3f(8, 8, 8), 0, 0, 0);
        head->set_pos(Vec3f(0, 0, 0));
        body = add_box(Vec3f(-4, 0, -2), Vec3f(8, 12, 4), 16, 16, 0);
        body->set_pos(Vec3f(0, 0, 0));
        left_arm = add_box(Vec3f(-1, -2, -2), Vec3f(4, 12, 4), 40, 16, 0);
        left_arm->set_pos(Vec3f(5, 2, 0));
        right_arm = add_box(Vec3f(-3, -2, -2), Vec3f(4, 12, 4), 40, 16, 0);
        right_arm->set_pos(Vec3f(-5, 2, 0));
        left_leg = add_box(Vec3f(-2, 0, -2), Vec3f(4, 12, 4), 0, 16, 0);
        left_leg->set_pos(Vec3f(-2, 12, 0));
        right_leg = add_box(Vec3f(-2, 0, -2), Vec3f(4, 12, 4), 0, 16, 0);
        right_leg->set_pos(Vec3f(2, 12, 0));
    }

    void render(vfloat_t distance, float partial_ticks, bool transparency) override
    {
        float arm_rot = std::cos(distance * 0.6662f) * speed / M_DTOR;
        float arm_rot2 = std::cos(distance * 0.6662f + M_PI) * speed / M_DTOR;
        head->set_rot(head_rot - rot);
        left_arm->set_rot(Vec3f(arm_rot, 0, 0));
        right_arm->set_rot(Vec3f(arm_rot2, 0, 0));
        left_leg->set_rot(Vec3f(arm_rot * 1.4, 0, 0));
        right_leg->set_rot(Vec3f(arm_rot2 * 1.4, 0, 0));

        // The player should not render in the transparent pass
        if (!transparency)
            Model::render(distance, partial_ticks, transparency);

        gertex::GXState state = gertex::get_state();
        render_handitem(right_arm, equipment[0], Vec3f(0, 0, 0), Vec3f(0, 0, 0), Vec3f(1.0), transparency);
        gertex::set_state(state);
    }
};

#endif