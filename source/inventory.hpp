#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <cstdint>
#include <functional>
#include <array>

#include "vec3i.hpp"
#include "entity.hpp"

namespace inventory
{
    enum tool_type : uint8_t
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

    enum tool_tier : uint8_t
    {
        wood,
        gold,
        stone,
        iron,
        diamond,
    };

    class item;
    void default_on_use(item &item, vec3i pos, vec3i face, aabb_entity_t *entity);

    void init_items();

    class item
    {
    public:
        uint16_t id = 0;
        uint8_t max_stack = 64;
        tool_type tool = none;
        tool_tier tier = wood;
        std::function<void(item &, vec3i, vec3i, aabb_entity_t *)> on_use = default_on_use;

        item(uint16_t id = 0, uint8_t max_stack = 64, std::function<void(item &, vec3i, vec3i, aabb_entity_t *)> on_use = default_on_use) : id(id), max_stack(max_stack), on_use(on_use) {}

        bool is_block() const
        {
            return id < 0x100;
        }
    };

    extern item item_list[512];

    class item_stack
    {
    public:
        uint16_t id;
        uint8_t count;
        uint8_t meta;

        item_stack(uint16_t id = 0, uint8_t count = 1, uint8_t meta = 0) : id(id), count(count), meta(meta) {}

        item &as_item() const
        {
            return item_list[id];
        }
    };

    class container
    {
    private:
        std::vector<item_stack> stacks;

    public:
        container(size_t size)
        {
            stacks.resize(size);
        }

        item_stack &operator[](size_t index)
        {
            return stacks[index];
        }

        /**
         * Clears the container i.e. sets all item_stacks to empty
         */
        void clear();

        /**
         * @return The maximum number of item_stacks that can be stored
         */
        size_t size() const;

        /**
         * @return The number of item_stacks stored
         */
        size_t count() const;

        /**
         * Adds the item_stack to the container
         * If there's no space, it will return the remaining stack.
         * @param stack the item_stack to add
         * @return the remaining stack
         */
        item_stack add(item_stack stack);

        /**
         * Inserts the item_stack at the specified index
         * @param stack the item_stack to insert
         * @param index the index to insert the item_stack
         */
        void replace(item_stack stack, size_t index);

        /**
         * Places the item at the specified index
         * If it's not stackable, it will replace the original item_stack and return the original item_stack
         * If it's stackable, it will add to the stack. If the stack is full, it will return the remaining stack
         * @param item the item to place
         * @param index the index to place the item
         */
        item_stack place(item_stack item, size_t index);
    };
} // namespace inventory
#endif