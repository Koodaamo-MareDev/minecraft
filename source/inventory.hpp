#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <cstdint>
#include <functional>
#include <array>

#include "vec3i.hpp"
#include "block_id.hpp"
#include <stdexcept>

class aabb_entity_t;

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
        uint8_t texture_index = 0;
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
        uint16_t meta;

        item_stack(uint16_t id = 0, uint8_t count = 0, uint16_t meta = 0) : id(id), count(count), meta(meta) {}
        item_stack(BlockID id, uint8_t count = 0, uint16_t meta = 0) : id(uint8_t(id)), count(count), meta(meta) {}

        item &as_item() const
        {
            return item_list[id & 0x1FF];
        }

        bool empty()
        {
            if (count == 0)
            {
                // Nullify the item_stack just in case
                id = 0;
                meta = 0;
                return true;
            }
            return id == 0;
        }
    };

    class container
    {
    private:
        std::vector<item_stack> stacks;
        uint32_t usable_slots;

    public:
        container(size_t size, uint32_t usable_slots) : usable_slots(usable_slots)
        {
            if (size < usable_slots)
            {
                throw std::invalid_argument("Container usable_slots cannot be greater than size");
            }
            stacks.resize(size);
        }
        container(size_t size) : container(size, size) {}

        item_stack &operator[](size_t index)
        {
            if (index >= stacks.size())
            {
                throw std::out_of_range("Index out of range");
            }
            return stacks[index];
        }

        /**
         * Clears the container i.e. sets all item_stacks to empty
         */
        void clear();

        /**
         * @return The maximum number of item_stacks that can be stored
         */
        size_t size();

        /**
         * @return The number of item_stacks stored
         */
        size_t count();

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