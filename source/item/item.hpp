#ifndef ITEM_HPP
#define ITEM_HPP

#include <functional>
#include <block/block_id.hpp>

class EntityPhysical;
class Vec3i;

namespace item
{
    enum ToolType : uint8_t
    {
        none,
        pickaxe,
        axe,
        shovel,
        hoe,
        sword,
        shears,
        bow,
    };

    enum ToolTier : uint8_t
    {
        no_tier,
        wood,
        gold,
        stone,
        iron,
        diamond,
    };
    class Item;

    void default_on_use(Item &item, const Vec3i &pos, const Vec3i &face, EntityPhysical *entity);

    class Item
    {
    public:
        int16_t id = 0;
        uint8_t max_stack = 64;
        uint8_t texture_index = 0;
        ToolType tool = none;
        ToolTier tier = wood;
        int32_t damage = 0;
        std::function<void(Item &, const Vec3i &, const Vec3i &, EntityPhysical *)> on_use = default_on_use;

        Item(int16_t id = 0, uint8_t texture_index = 0, uint8_t max_stack = 64, std::function<void(Item &, const Vec3i &, const Vec3i &, EntityPhysical *)> on_use = default_on_use) : id(id), max_stack(max_stack), texture_index(texture_index), on_use(on_use) {}

        bool is_block() const
        {
            return id < 0x100;
        }

        Item set_tool_properties(ToolType type, ToolTier tier, int32_t damage)
        {
            this->tool = type;
            this->tier = tier;
            this->damage = damage;
            return *this;
        }

        float get_efficiency(const BlockID &block_id, ToolType type, ToolTier tier) const;
    };

    extern Item item_list[2560];
} // namespace item

#endif