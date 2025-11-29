#include "item.hpp"

#include "item_id.hpp"
#include "math/vec3i.hpp"

namespace item
{

    Item item_list[2560];

    void default_on_use(Item &item, const Vec3i &pos, const Vec3i &face, EntityPhysical *entity)
    {
        // Do nothing
    }

    float Item::get_efficiency(const BlockID &block_id, ToolType block_tool_type, ToolTier block_tool_tier) const
    {
        // Swords have a fixed efficiency
        if (this->tool == ToolType::sword)
            return 1.5f;

        // Skip if the item is not a tool
        if (this->tier == ToolTier::no_tier || this->tool == ToolType::none)
            return 1.0f;

        // Apply the efficiency based on tier if the tool matches
        if (this->tool == block_tool_type && this->tier >= block_tool_tier)
        {
            switch (this->tier)
            {
            case ToolTier::wood:
                return 2.0f;
            case ToolTier::gold:
                return 12.0f;
            case ToolTier::stone:
                return 4.0f;
            case ToolTier::iron:
                return 6.0f;
            case ToolTier::diamond:
                return 8.0f;
            default:
                break;
            }
        }

        return 1.0f;
    }

} // namespace item
