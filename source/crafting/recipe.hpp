#ifndef RECIPE_HPP
#define RECIPE_HPP

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <item/inventory.hpp>

namespace crafting
{
    class Input
    {
    public:
        size_t width = 0;
        size_t height = 0;
        std::vector<inventory::ItemStack> items;

        Input(size_t width, size_t height, const std::vector<inventory::ItemStack> &items)
        {
            if (width * height != items.size())
                throw std::runtime_error("Recipe size mismatch");
            trim(width, height, items);
        }

        inventory::ItemStack operator[](size_t i)
        {
            return items[i];
        }

        size_t size()
        {
            return items.size();
        }

        Input sorted();

    private:
        void trim(size_t width, size_t height, const std::vector<inventory::ItemStack> &items);
    };

    class Recipe
    {
    public:
        inventory::ItemStack result;
        virtual bool matches(Input &input) = 0;
    };

    class ShapedRecipe : public Recipe
    {
    public:
        Input recipe;
        ShapedRecipe(Input recipe, const inventory::ItemStack &result) : recipe(recipe)
        {
            this->result = result;
        }
        bool matches(Input &input);
    };

    class ShapelessRecipe : public Recipe
    {
    public:
        std::vector<inventory::ItemStack> recipe;
        ShapelessRecipe(Input recipe, const inventory::ItemStack &result)
        {
            this->result = result;
            this->recipe = recipe.sorted().items;
        }
        bool matches(Input &input);
    };

} // namespace crafting
#endif