#ifndef ITEM_STACK_HPP
#define ITEM_STACK_HPP

#include <cstdint>
#include <block/block_id.hpp>

class NBTTagCompound;
enum BlockID;

namespace item
{
    class Item;
    class ItemStack
    {
    public:
        int16_t id;
        uint8_t count;
        int16_t meta;

        ItemStack(int16_t id = 0, uint8_t count = 0, int16_t meta = 0) : id(id), count(count), meta(meta) {}
        ItemStack(const BlockID &id, uint8_t count = 0, int16_t meta = 0) : id(uint8_t(id)), count(count), meta(meta) {}
        ItemStack(NBTTagCompound *nbt);

        Item &as_item() const;

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

        uint8_t get_texture_index() const;

        /**
         * Converts the item_stack to an NBT compound tag
         * @return NBTTagCompound* the NBT compound tag
         * @note The returned NBTTagCompound* must be deleted by the caller
         */
        NBTTagCompound *serialize();
    };
} // namespace item

#endif