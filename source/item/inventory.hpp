#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <cstdint>
#include <functional>
#include <array>
#include <stdexcept>
#include <math/vec3i.hpp>
#include <vector>

#include <block/block_id.hpp>
#include <item/item_stack.hpp>
#include <nbt/nbt.hpp>

class EntityPhysical;

namespace inventory
{
    class Container
    {
    private:
        uint32_t usable_slots;

    protected:
        std::vector<item::ItemStack> stacks;

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

        item::ItemStack &operator[](size_t index)
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
        virtual item::ItemStack add(item::ItemStack stack);

        /**
         * Inserts the item_stack at the specified index
         * @param stack the item_stack to insert
         * @param index the index to insert the item_stack
         */
        virtual void replace(item::ItemStack stack, size_t index);

        /**
         * Places the item at the specified index
         * If it's not stackable, it will replace the original item_stack and return the original item_stack
         * If it's stackable, it will add to the stack. If the stack is full, it will return the remaining stack
         * @param item the item to place
         * @param index the index to place the item
         */
        virtual item::ItemStack place(item::ItemStack item, size_t index);

        /**
         * Finds a free slot for the item_stack
         * It will first try to find a stack with the same id and meta that isn't full
         * If it can't find one, it will return the first empty slot
         * If there's no space, it will return -1
         * @param stack the item_stack to find a free slot for
         * @return the index of the free slot, or -1 if there's no space
         */
        virtual int find_free_slot_for(item::ItemStack stack);

        /**
         * Converts the Container to an NBT list tag
         * @return NBTTagList* the NBT list tag
         * @note The returned NBTTagList* must be deleted by the caller
         */
        virtual NBTTagList *serialize();

        /**
         * Loads the Container from an NBT list tag
         * @param nbt the NBT list tag
         */
        virtual void deserialize(NBTTagList *nbt);
    };

    class PlayerInventory : public Container
    {
    public:
        PlayerInventory(size_t inventory_size) : Container(inventory_size) {}

        virtual int find_free_slot_for(item::ItemStack stack) override;

        virtual NBTTagList *serialize() override;

        virtual void deserialize(NBTTagList *nbt) override;
    };
} // namespace inventory
#endif