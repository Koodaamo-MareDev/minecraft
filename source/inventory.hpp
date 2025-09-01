#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <cstdint>
#include <functional>
#include <array>
#include <stdexcept>
#include <math/vec3i.hpp>
#include <vector>

#include "block_id.hpp"
#include "item_id.hpp"

class EntityPhysical;

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
        no_tier,
        wood,
        gold,
        stone,
        iron,
        diamond,
    };

    class Item;
    void default_on_use(Item &item, Vec3i pos, Vec3i face, EntityPhysical *entity);

    void init_items();

    class Item
    {
    public:
        int16_t id = 0;
        uint8_t max_stack = 64;
        uint8_t texture_index = 0;
        tool_type tool = none;
        tool_tier tier = wood;
        int32_t damage = 0;
        std::function<void(Item &, Vec3i, Vec3i, EntityPhysical *)> on_use = default_on_use;

        Item(int16_t id = 0, uint8_t texture_index = 0, uint8_t max_stack = 64, std::function<void(Item &, Vec3i, Vec3i, EntityPhysical *)> on_use = default_on_use) : id(id), max_stack(max_stack), texture_index(texture_index), on_use(on_use) {}

        bool is_block() const
        {
            return id < 0x100;
        }

        Item set_tool_properties(tool_type type, tool_tier tier, int32_t damage)
        {
            this->tool = type;
            this->tier = tier;
            this->damage = damage;
            return *this;
        }

        float get_efficiency(BlockID block_id, tool_type type, tool_tier tier) const;
    };

    extern Item item_list[2560];

    class ItemStack
    {
    public:
        int16_t id;
        uint8_t count;
        int16_t meta;

        ItemStack(int16_t id = 0, uint8_t count = 0, int16_t meta = 0) : id(id), count(count), meta(meta) {}
        ItemStack(BlockID id, uint8_t count = 0, int16_t meta = 0) : id(uint8_t(id)), count(count), meta(meta) {}

        Item &as_item() const
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

        uint8_t get_texture_index() const
        {
            if (this->id == +ItemID::dye)
            {
                // Remap color index to the 2x8 portion in the texture atlas
                uint8_t base_index = as_item().texture_index;
                uint8_t color_index = (this->meta >> 3) | ((this->meta & 0x7) << 4);
                return base_index + color_index;
            }
            return as_item().texture_index;
        }
    };

    class Container
    {
    private:
        uint32_t usable_slots;

    protected:
        std::vector<ItemStack> stacks;

    public:
        Container(size_t size, uint32_t usable_slots) : usable_slots(usable_slots)
        {
            if (size < usable_slots)
            {
                throw std::invalid_argument("Container usable_slots cannot be greater than size");
            }
            stacks.resize(size);
        }
        Container(size_t size) : Container(size, size) {}

        ItemStack &operator[](size_t index)
        {
            if (index >= stacks.size())
            {
                throw std::out_of_range("Index out of range");
            }
            return stacks[index];
        }

        /**
         * Clears the Container i.e. sets all item_stacks to empty
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
         * Adds the item_stack to the Container
         * If there's no space, it will return the remaining stack.
         * @param stack the item_stack to add
         * @return the remaining stack
         */
        virtual ItemStack add(ItemStack stack);

        /**
         * Inserts the item_stack at the specified index
         * @param stack the item_stack to insert
         * @param index the index to insert the item_stack
         */
        virtual void replace(ItemStack stack, size_t index);

        /**
         * Places the item at the specified index
         * If it's not stackable, it will replace the original item_stack and return the original item_stack
         * If it's stackable, it will add to the stack. If the stack is full, it will return the remaining stack
         * @param item the item to place
         * @param index the index to place the item
         */
        virtual ItemStack place(ItemStack item, size_t index);

        virtual int find_free_slot_for(ItemStack stack);
    };

    class PlayerInventory : public Container
    {
    public:
        PlayerInventory(size_t inventory_size) : Container(inventory_size) {}

        virtual int find_free_slot_for(ItemStack stack) override;
    };
} // namespace inventory
#endif