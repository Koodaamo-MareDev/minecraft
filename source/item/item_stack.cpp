#include "item_stack.hpp"
#include "item.hpp"
#include "item_id.hpp"
#include <block/block_id.hpp>
#include <nbt/nbt.hpp>

namespace item
{
    ItemStack::ItemStack(NBTTagCompound *nbt)
    {
        if (nbt)
        {
            this->id = nbt->getShort("id");
            this->count = nbt->getByte("Count");
            this->meta = nbt->getShort("Damage");
        }
        else
        {
            this->id = 0;
            this->count = 0;
            this->meta = 0;
        }
    }

    Item &ItemStack::as_item() const
    {
        return item_list[id];
    }

    uint8_t ItemStack::get_texture_index() const
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
    NBTTagCompound *ItemStack::serialize()
    {
        NBTTagCompound *nbt = new NBTTagCompound;
        nbt->setTag("id", new NBTTagShort(id));
        nbt->setTag("Count", new NBTTagByte(count));
        nbt->setTag("Damage", new NBTTagShort(meta));
        return nbt;
    }
} // namespace item
