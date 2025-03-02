#include "inventory.hpp"
namespace inventory
{
    void init_items()
    {
        for (int i = 0; i < 512; i++)
        {
            item_list[i].id = i;
        }
        item_list[256].texture_index = 82;
        item_list[257].texture_index = 98;
    }

    item item_list[512];

    void default_on_use(item &item, vec3i pos, vec3i face, entity_physical *entity)
    {
        // Do nothing
    }

    void container::clear()
    {
        for (size_t i = 0; i < size(); i++)
        {
            stacks[i] = item_stack();
        }
    }

    size_t container::size()
    {
        return stacks.size();
    }

    size_t container::count()
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

    item_stack container::add(item_stack stack)
    {
        // Search for stacks with the same id and meta
        for (size_t i = 0; i < size() && i < usable_slots; i++)
        {
            item_stack &current = stacks[i];
            size_t max_stack = current.as_item().max_stack;

            // Skip full stacks
            if (current.count == max_stack)
            {
                continue;
            }

            if (current.id == stack.id && current.meta == stack.meta)
            {
                // Add the stack to the current stack
                current.count += stack.count;
                if (current.count > max_stack)
                {
                    stack.count = current.count - max_stack;
                    current.count = max_stack;
                    continue;
                }
                return item_stack();
            }
        }

        // Place the stack in the first empty slot
        for (size_t i = 0; i < size() && i < usable_slots; i++)
        {
            if (stacks[i].empty())
            {
                stacks[i] = stack;
                return item_stack();
            }
        }

        // Return the remaining stack if there's no space
        return stack;
    }

    void container::replace(item_stack stack, size_t index)
    {
        if (index < size())
        {
            stacks[index] = stack;
        }
    }

    item_stack container::place(item_stack stack, size_t index)
    {
        if (index < size())
        {
            item_stack &orig = stacks[index];

            // If the stack is empty, just place the stack
            if (orig.id == 0)
            {
                orig = stack;
                return item_stack();
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
                return item_stack();
            }

            // Swap the stacks
            item_stack temp = orig;
            orig = stack;
            return temp;
        }

        // Don't place the stack if the index is out of bounds
        return stack;
    }
} // namespace inventory