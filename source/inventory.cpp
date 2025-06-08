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

    float item::get_efficiency(BlockID block_id, tool_type block_tool_type, tool_tier block_tool_tier) const
    {
        // Swords have a fixed efficiency
        if (tool == tool_type::sword)
            return 1.5f;

        // Skip if the item is not a tool
        if (block_tool_tier == tool_tier::no_tier || block_tool_type == tool_type::none)
            return 1.0f;

        // Apply the efficiency based on tier if the tool matches
        if (this->tool == block_tool_type && this->tier >= block_tool_tier)
        {
            switch (this->tier)
            {
            case tool_tier::wood:
                return 2.0f;
            case tool_tier::gold:
                return 12.0f;
            case tool_tier::stone:
                return 4.0f;
            case tool_tier::iron:
                return 6.0f;
            case tool_tier::diamond:
                return 8.0f;
            default:
                break;
            }
        }

        return 1.0f;
    }
} // namespace inventory