#include "recipe_manager.hpp"
#include <fstream>
#include <thirdparty/nlohmann/json.hpp>
#include <util/debuglog.hpp>
#include <util/constants.hpp>
#include <util/string_utils.hpp>

namespace crafting
{

    RecipeManager::RecipeManager()
    {
        namespace fs = std::filesystem;

        std::string old_path = fs::current_path();
        fs::current_path(RESOURCES_DIR);

        load_id_mappings();

        load_all_recipes();

        fs::current_path(old_path);

        // We don't need these anymore.
        id_mappings.clear();
    }

    RecipeManager &RecipeManager::instance()
    {
        static RecipeManager inst;
        return inst;
    }

    void RecipeManager::load_all_recipes()
    {
        namespace fs = std::filesystem;
        for (const auto &entry : fs::directory_iterator("recipes"))
        {
            if (entry.is_regular_file())
            {
                std::string path = entry.path();
                try
                {
                    load_recipe(path);
                }
                catch (std::runtime_error &e)
                {
                    debug::print("[RecipeManager] In file %s: %s", path.c_str(), e.what());
                }
            }
        }
    }

    void RecipeManager::load_recipe(std::string file_path)
    {
        auto name_to_id = [&](std::string name) -> int16_t
        {
            name = name.substr(name.find(':') + 1);
            if (auto mapping = id_mappings.find(name); mapping != id_mappings.end())
            {
                return mapping->second;
            }
            throw std::runtime_error("No id mapping for " + name);
        };

        auto json_to_item = [&](nlohmann::json item) -> inventory::ItemStack
        {
            int16_t id = 0;
            int16_t meta = 0;
            uint8_t count = 1;

            // The item should at least contain the id.
            if (item.contains("item"))
                id = name_to_id(item["item"]);
            else
                throw std::runtime_error("Missing item id");

            // The meta data and the count are optional.
            if (item.contains("data"))
                meta = item["data"];

            if (item.contains("count"))
                count = item["count"];

            return inventory::ItemStack(id, count, meta);
        };

        std::ifstream file(file_path);
        if (!file.is_open())
            throw std::runtime_error("Failed to open file");

        nlohmann::json recipe_json;
        file >> recipe_json;
        std::string type = recipe_json["type"];

        if (type == "crafting_shapeless")
        {
            nlohmann::json ingredients = recipe_json["ingredients"];

            std::vector<inventory::ItemStack> inputs;
            for (nlohmann::json ingredient : ingredients)
            {
                // TODO: Handle alternative ingredients properly
                // for items like torch that requires coal OR charcoal.
                if (ingredient.is_array())
                    ingredient = ingredient[0];

                inputs.push_back(json_to_item(ingredient));
            }
            nlohmann::json result = recipe_json["result"];

            inventory::ItemStack result_item = json_to_item(result);

            shapeless_recipes.push_back(ShapelessRecipe(Input(1, inputs.size(), inputs), result_item));
        }
        else if (type == "crafting_shaped")
        {
            std::map<char, inventory::ItemStack> item_keys;
            nlohmann::json ingredient_keys = recipe_json["key"];
            for (auto &[key, ingredient] : ingredient_keys.items())
            {
                if (key.size() != 1)
                    throw std::runtime_error("Length of key '" + key + "' != 1 in recipe " + file_path);

                // TODO: Handle alternative ingredients properly
                // for items like torch that requires coal OR charcoal.
                if (ingredient.is_array())
                    ingredient = ingredient[0];

                item_keys[key[0]] = json_to_item(ingredient);
            }

            nlohmann::json pattern = recipe_json["pattern"];

            size_t height = pattern.size();
            size_t width = 0;
            std::vector<inventory::ItemStack> recipe;
            for (std::string row : pattern)
            {
                // Ensure consistency of the width.
                size_t current_width = row.size();
                if (width == 0)
                    width = current_width;
                else if (width != current_width)
                    throw std::runtime_error("Inconsistent width in recipe " + file_path);

                // Map the characters of the row to the corresponding items.
                for (char c : row)
                {
                    if (auto it = item_keys.find(c); it != item_keys.end())
                        recipe.push_back(it->second);
                    else
                        recipe.push_back(inventory::ItemStack());
                }
            }
            nlohmann::json result = recipe_json["result"];

            inventory::ItemStack result_item = json_to_item(result);

            shaped_recipes.push_back(ShapedRecipe(Input(width, height, recipe), result_item));
        }
    }

    // See https://minecraft-ids.grahamedgecombe.com/api
    void RecipeManager::load_id_mappings()
    {
        std::ifstream file("items.tsv");
        if (!file.is_open())
            throw std::runtime_error("Can't load item id mappings.");

        std::string line;
        while (std::getline(file, line))
        {
            std::vector<std::string> parts = str::split(line, '\t');
            id_mappings[parts[3]] = std::stoi(parts[0]);
        }
    }

    void RecipeManager::clear()
    {
        shaped_recipes.clear();
        shapeless_recipes.clear();
        id_mappings.clear();
    }

    inventory::ItemStack RecipeManager::craft(Input &input)
    {
        // Try shaped recipes first.
        for (ShapedRecipe &recipe : shaped_recipes)
        {
            if (recipe.matches(input))
                return recipe.result;
        }

        // If not found, fallback to shapeless.
        Input sorted_input = input.sorted();
        for (ShapelessRecipe &recipe : shapeless_recipes)
        {
            if (recipe.matches(sorted_input))
                return recipe.result;
        }

        // No matches found.
        return inventory::ItemStack();
    }

} // namespace crafting
