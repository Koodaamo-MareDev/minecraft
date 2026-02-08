#include "recipe.hpp"
#include <unordered_map>

namespace crafting
{
    // The recipe manager uses the same recipe json format as seen in 1.12.x
    // This is for convenience as it's the only version prior to the flattening
    // where recipes with item meta exist as json.
    class RecipeManager
    {
    public:
        static RecipeManager &instance();

        void load_all_recipes();
        void load_recipe(std::string file_path);
        void load_id_mappings();
        void clear();

        item::ItemStack craft(Input &input);
        item::ItemStack smelt(item::ItemStack input);

    private:
        RecipeManager();
        std::unordered_map<std::string, int16_t> id_mappings;
        std::vector<ShapedRecipe> shaped_recipes;
        std::vector<ShapelessRecipe> shapeless_recipes;
        std::vector<FurnaceRecipe> furnace_recipes;
    };
} // namespace crafting
