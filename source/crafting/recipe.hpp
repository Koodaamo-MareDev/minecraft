#ifndef RECIPE_HPP
#define RECIPE_HPP

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <item/item_stack.hpp>

namespace crafting
{
    class Input
    {
    public:
        size_t width = 0;
        size_t height = 0;
        std::vector<item::ItemStack> items;

        Input(size_t width, size_t height, const std::vector<item::ItemStack> &items)
        {
            if (width * height != items.size())
                throw std::runtime_error("Recipe size mismatch");
            trim(width, height, items);
        }

        item::ItemStack operator[](size_t i)
        {
            return items[i];
        }

        size_t size()
        {
            return items.size();
        }

        Input sorted();

    private:
        void trim(size_t width, size_t height, const std::vector<item::ItemStack> &items);
    };

    class Recipe
    {
    public:
        item::ItemStack result;
        virtual bool matches(Input &input) = 0;
    };

    class ShapedRecipe : public Recipe
    {
    public:
        Input recipe;
        ShapedRecipe(Input recipe, const item::ItemStack &result) : recipe(recipe)
        {
            this->result = result;
        }
        bool matches(Input &input);
    };

    class ShapelessRecipe : public Recipe
    {
    public:
        std::vector<item::ItemStack> recipe;
        ShapelessRecipe(Input recipe, const item::ItemStack &result)
        {
            this->result = result;
            this->recipe = recipe.sorted().items;
        }
        bool matches(Input &input);
    };

} // namespace crafting
#endif