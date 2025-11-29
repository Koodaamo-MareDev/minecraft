#include "inventory.hpp"
#include <item/item.hpp>

namespace inventory
{
    void Container::clear()
    {
        for (size_t i = 0; i < size(); i++)
        {
            stacks[i] = item::ItemStack();
        }
    }

    size_t Container::size()
    {
        return stacks.size();
    }

    size_t Container::count()
    {
        size_t count = 0;
        for (size_t i = 0; i < size(); i++)
        {
            if (!stacks[i].empty())
            {
                count++;
            }
        }
        return count;
    }

    item::ItemStack Container::add(item::ItemStack stack)
    {
        if (stack.empty())
            return stack;

        while (stack.count > 0)
        {
            int slot = find_free_slot_for(stack);

            if (slot == -1)
                break;

            stack = place(stack, slot);
        }

        // Return the remaining stack if there's no space
        return stack;
    }

    void Container::replace(item::ItemStack stack, size_t index)
    {
        if (index < size())
        {
            stacks[index] = stack;
        }
    }

    item::ItemStack Container::place(item::ItemStack stack, size_t index)
    {
        if (index < size())
        {
            item::ItemStack &orig = stacks[index];

            // If the stack is empty, just place the stack
            if (orig.id == 0)
            {
                orig = stack;
                return item::ItemStack();
            }

            // If the stack is the same, add the stack
            if (orig.id == stack.id && orig.meta == stack.meta)
            {
                orig.count += stack.count;

                // If the stack is over the max stack size, split the stack
                if (orig.count > orig.as_item().max_stack)
                {
                    stack.count = orig.count - orig.as_item().max_stack;
                    orig.count = orig.as_item().max_stack;

                    // Return the remaining stack
                    return stack;
                }

                // Return an empty stack
                return item::ItemStack();
            }

            // Swap the stacks
            item::ItemStack temp = orig;
            orig = stack;
            return temp;
        }

        // Don't place the stack if the index is out of bounds
        return stack;
    }

    int Container::find_free_slot_for(item::ItemStack stack)
    {
        // If the stack is empty, return -1
        if (stack.empty())
            return -1;

        // Search for stacks with the same id and meta
        for (size_t i = 0; i < size() && i < usable_slots; i++)
        {
            item::ItemStack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count >= max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                return i;
            }
        }

        // Place the stack in the first empty slot
        for (size_t i = 0; i < size() && i < usable_slots; i++)
        {
            if (stacks[i].empty())
            {
                return i;
            }
        }

        // No free slot found
        return -1;
    }

    int PlayerInventory::find_free_slot_for(item::ItemStack stack)
    {
        // If the stack is empty, return -1
        if (stack.empty() || size() < 45)
            return -1;

        // Search hotbar for stacks with the same id and meta (starting at 36 as the hotbar starts there)
        for (size_t i = 36; i < 45; i++)
        {
            item::ItemStack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count >= max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                return i;
            }
        }

        // Place the stack in the first empty inventory slot (starting at 36)
        for (size_t i = 36; i < 45; i++)
        {
            if (stacks[i].empty())
            {
                return i;
            }
        }

        // Search inventory for stacks with the same id and meta (starting at 9 as the first 9 slots are reserved for armor and crafting)
        for (size_t i = 9; i < 36; i++)
        {
            item::ItemStack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count >= max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                return i;
            }
        }

        // Place the stack in the first empty inventory slot (starting at 9)
        for (size_t i = 9; i < 36; i++)
        {
            if (stacks[i].empty())
            {
                return i;
            }
        }

        // No free slot found
        return -1;
    }

    NBTTagList *Container::serialize()
    {
        NBTTagList *nbt = new NBTTagList();

        for (size_t i = 0; i < size(); i++)
        {
            item::ItemStack &stack = stacks[i];
            if (!stack.empty())
            {
                NBTTagCompound *item_nbt = stack.serialize();
                item_nbt->setTag("Slot", new NBTTagByte(i));
                nbt->addTag(item_nbt);
            }
        }

        return nbt;
    }

    void Container::deserialize(NBTTagList *nbt)
    {
        clear();

        for (size_t i = 0; i < nbt->value.size() && i < stacks.size(); i++)
        {
            NBTTagCompound *item_nbt = dynamic_cast<NBTTagCompound *>(nbt->value[i]);
            if (item_nbt)
            {
                size_t slot = item_nbt->getByte("Slot") & 0xFF;
                if (slot < stacks.size())
                {
                    stacks[slot] = item::ItemStack(item_nbt);
                }
            }
        }
    }

    NBTTagList *PlayerInventory::serialize()
    {
        NBTTagList *nbt = new NBTTagList();

        // Inventory slots 9-44
        for (size_t i = 0; i < 36; i++)
        {
            item::ItemStack &stack = stacks[i + 9];
            if (!stack.empty())
            {
                NBTTagCompound *item_nbt = stack.serialize();
                item_nbt->setTag("Slot", new NBTTagByte(i));
                nbt->addTag(item_nbt);
            }
        }

        // Armor slots 5-8
        for (size_t i = 0; i < 4; i++)
        {
            item::ItemStack &stack = stacks[i + 5];
            if (!stack.empty())
            {
                NBTTagCompound *item_nbt = stack.serialize();
                item_nbt->setTag("Slot", new NBTTagByte(i + 100));
                nbt->addTag(item_nbt);
            }
        }

        return nbt;
    }

    void PlayerInventory::deserialize(NBTTagList *nbt)
    {
        clear();

        for (size_t i = 0; i < nbt->value.size() && i < stacks.size(); i++)
        {
            NBTTagCompound *item_nbt = dynamic_cast<NBTTagCompound *>(nbt->value[i]);
            if (item_nbt)
            {
                int slot = item_nbt->getByte("Slot");
                if (slot >= 0 && slot < 36)
                {
                    stacks[slot + 9] = item::ItemStack(item_nbt);
                }
                else if (slot >= 100 && slot < 104)
                {
                    stacks[slot - 95] = item::ItemStack(item_nbt);
                }
            }
        }
    }
} // namespace inventory