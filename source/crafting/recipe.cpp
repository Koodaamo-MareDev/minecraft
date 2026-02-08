#include "recipe.hpp"

namespace crafting
{
    Input Input::sorted()
    {
        Input result = *this;

        auto sorter = [](item::ItemStack &a, item::ItemStack &b) -> bool
        {
            return ((a.id << 16) | a.meta) < ((b.id << 16) | b.meta);
        };

        auto filter = [](item::ItemStack &a) -> bool
        {
            return a.id == 0;
        };

        result.items.erase(std::remove_if(result.items.begin(), result.items.end(), filter), result.items.end());

        std::sort(result.items.begin(), result.items.end(), sorter);

        return result;
    }

    void crafting::Input::trim(size_t width, size_t height, const std::vector<item::ItemStack> &input_items)
    {
        this->items = input_items;

        // Remove empty rows from beginning
        while (height > 0)
        {
            bool row_empty = true;
            for (size_t x = 0; x < width; x++)
            {
                if (!items[x].empty())
                {
                    row_empty = false;
                    break;
                }
            }
            if (!row_empty)
                break;

            // Remove the row
            items.erase(items.begin(), items.begin() + width);
            height--;
        }

        // Remove empty columns from beginning
        while (width > 0)
        {
            bool column_empty = true;
            for (size_t y = 0; y < height; y++)
            {
                if (!items[y * width].empty())
                {
                    column_empty = false;
                    break;
                }
            }
            if (!column_empty)
                break;

            // Remove the column
            for (size_t y = height; y > 0; y--)
            {
                items.erase(items.begin() + (y - 1) * width);
            }
            width--;
        }

        // Remove empty rows from the end
        while (height > 0)
        {
            bool row_empty = true;
            for (size_t x = items.size() - width; x < items.size(); x++)
            {
                if (!items[x].empty())
                {
                    row_empty = false;
                    break;
                }
            }
            if (!row_empty)
                break;

            // Remove the row
            items.erase(items.end() - width, items.end());
            height--;
        }

        // Remove empty columns from end
        while (width > 0)
        {
            bool column_empty = true;
            // Start from the next row...
            for (size_t y = 1; y <= height; y++)
            {
                // ... and subtract 1 to get to the previous row
                if (!items[y * width - 1].empty())
                {
                    column_empty = false;
                    break;
                }
            }
            if (!column_empty)
                break;

            // Remove the column
            for (size_t y = height; y > 0; y--)
            {
                items.erase(items.begin() + (y * width) - 1);
            }
            width--;
        }

        this->width = width;
        this->height = height;
    }

    bool ShapedRecipe::matches(Input &input)
    {
        if (input.width != recipe.width || input.height != recipe.height)
            return false;
        for (size_t i = 0; i < input.size(); i++)
        {
            item::ItemStack a = input[i];
            item::ItemStack b = recipe[i];
            if (a.id != b.id || a.meta != b.meta)
            {
                return false;
            }
        }
        return true;
    }

    bool ShapelessRecipe::matches(Input &input)
    {
        if (input.size() != recipe.size())
            return false;
        for (size_t i = 0; i < input.size(); i++)
        {
            item::ItemStack a = input[i];
            item::ItemStack b = recipe[i];
            if (a.id != b.id || a.meta != b.meta)
            {
                return false;
            }
        }
        return true;
    }

    bool FurnaceRecipe::matches(Input &input)
    {
        if (input.size() != 1)
            return false;
        return (input[0].id == this->recipe.id && input[0].meta == this->recipe.meta);
    }

} // namespace crafting