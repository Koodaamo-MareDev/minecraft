#include "input.hpp"
#include <wiiuse/wpad.h>
#include <functional>
#include <vector>
#include <algorithm>

namespace input
{
    std::vector<input::Device *> devices;
    std::map<std::string, uint8_t> string_joystick_src_mapping;
    std::map<std::string, uint8_t> string_joystick_dst_mapping;
    std::map<std::string, uint32_t> string_button_mapping;

    uint8_t cursor_joystick = JOY_LEFT;

    Configuration config;

    float deadzone = 0.1f;
    float sensitivity = 360.0f;

    // Ensure we do not initialize multiple times
    static bool initialized = false;

    void init()
    {
        if (initialized)
            return;
        string_joystick_dst_mapping = {
            {"joy_left", JOY_LEFT},
            {"joy_right", JOY_RIGHT},
            {"joy_aux", JOY_AUX}};

        string_button_mapping = {
            {"confirm", BUTTON_CONFIRM},
            {"jump", BUTTON_JUMP},
            {"inventory", BUTTON_INVENTORY},
            {"cancel", BUTTON_CANCEL},
            {"hotbar_left", BUTTON_HOTBAR_LEFT},
            {"hotbar_right", BUTTON_HOTBAR_RIGHT},
            {"mine", BUTTON_MINE},
            {"place", BUTTON_PLACE},
            {"toggle_sneak", BUTTON_TOGGLE_SNEAK},
            {"toggle_pause", BUTTON_TOGGLE_PAUSE},
            {"joy_left_up", JOYLEFT_UP},
            {"joy_left_down", JOYLEFT_DOWN},
            {"joy_left_left", JOYLEFT_LEFT},
            {"joy_left_right", JOYLEFT_RIGHT},
            {"joy_right_up", JOYRIGHT_UP},
            {"joy_right_down", JOYRIGHT_DOWN},
            {"joy_right_left", JOYRIGHT_LEFT},
            {"joy_right_right", JOYRIGHT_RIGHT}};

        reload_config();

        // All done.
        initialized = true;
    }

    void reload_config()
    {
        try
        {
            config.load("/apps/minecraft/inputconfig.txt");
        }
        catch (std::runtime_error &e)
        {
            debug::print("Failed to reload input config: %s\n", e.what());
        }
        deadzone = std::min(float(config.get("joystick_deadzone", 0.1f)), 0.5f);
        sensitivity = config.get("look_sensitivity", 360.0f);

        // The joystick used for the cursor is handled separately
        std::string cursor_joystick_binding = config.get<std::string>("cursor_joystick", "");
        if (string_joystick_dst_mapping.find(cursor_joystick_binding) != string_joystick_dst_mapping.end())
        {
            cursor_joystick = string_joystick_dst_mapping[cursor_joystick_binding];
        }
    }

    void deinit()
    {
        for (Device *dev : devices)
        {
            delete dev;
        }
        devices.clear();
        config.save("/apps/minecraft/inputconfig.txt");
    }

    void add_device(Device *dev)
    {
        devices.push_back(dev);
    }

} // namespace input
