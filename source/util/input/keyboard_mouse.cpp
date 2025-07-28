#include "keyboard_mouse.hpp"

namespace input
{
    static bool initialized = false;
    static std::map<std::string, uint8_t> string_mouse_mapping;
    static uint8_t joystick_src_mouse_move;
    static uint8_t joystick_src_mouse_scroll;

    void keyboard_mouse::init()
    {
        if (initialized)
            return;

        string_mouse_mapping["mouse left"] = MOUSE_BUTTON_LEFT;
        string_mouse_mapping["mouse right"] = MOUSE_BUTTON_RIGHT;
        string_mouse_mapping["mouse middle"] = MOUSE_BUTTON_MIDDLE;
        string_mouse_mapping["mouse wheel up"] = MOUSE_BUTTON_WHEEL_UP;
        string_mouse_mapping["mouse wheel down"] = MOUSE_BUTTON_WHEEL_DOWN;

        joystick_src_mouse_move = string_joystick_src_mapping["mouse move"] = string_joystick_src_mapping.size();
        joystick_src_mouse_scroll = string_joystick_src_mapping["mouse scroll"] = string_joystick_src_mapping.size();
        initialized = true;
    }

    keyboard_mouse::keyboard_mouse()
    {
        init();
        KEYBOARD_Init(NULL);
        MOUSE_Init();

        // Load joystick bindings from configuration
        for (const auto &pair : string_joystick_dst_mapping)
        {
            std::string binding = config.get<std::string>(pair.first, "");
            if (binding.empty())
                continue;
            if (string_joystick_src_mapping.find(binding) != string_joystick_src_mapping.end())
            {
                joystick_bindings[pair.second] = string_joystick_src_mapping[binding];
            }
            else
            {
                debug::print("Skipping joystick binding for '%s': unknown source '%s'\n", pair.first.c_str(), binding.c_str());
            }
        }

        // Initialize button map from configuration
        for (const auto &pair : string_button_mapping)
        {
            std::string binding = config.get<std::string>(pair.first, "");

            // If the binding is empty, skip it
            if (binding.empty())
                continue;

            // Check if the binding is a mouse button
            if (string_mouse_mapping.find(binding) != string_mouse_mapping.end())
            {
                mouse_button_bindings[string_mouse_mapping[binding]] = pair.second;
                continue;
            }

            // Fallback to keyboard bindings. See the USB HID Usage Tables for keycodes
            // https://github.com/tmk/tmk_keyboard/wiki/USB:-HID-Usage-Table

            int32_t hid_binding = config.get<int32_t>(pair.first, 0);

            // 0 means unbound, so skip it
            if (hid_binding == 0)
                continue;

            // Add the binding to the keyboard bindings
            keyboard_bindings[hid_binding] = pair.second;
        }
    }

    keyboard_mouse::~keyboard_mouse()
    {
        KEYBOARD_Deinit();
        MOUSE_Deinit();
    }

    void keyboard_mouse::scan()
    {
        buttons_down = 0;
        joysticks[JOY_LEFT] = vec3f(0);
        joysticks[JOY_RIGHT] = vec3f(0);
        joysticks[JOY_AUX] = vec3f(0);

        // Get keyboard events
        keyboard_event event;
        while (KEYBOARD_GetEvent(&event))
        {
            // Process keyboard event
            switch (event.type)
            {
            case KEYBOARD_CONNECTED:
                keyboard_connected = true;
                debug::print("Keyboard connected\n");
                break;
            case KEYBOARD_DISCONNECTED:
                keyboard_connected = false;
                debug::print("Keyboard disconnected\n");
                buttons_down = 0;
                buttons_held = 0;
                joysticks[JOY_LEFT] = vec3f(0);
                joysticks[JOY_RIGHT] = vec3f(0);
                joysticks[JOY_AUX] = vec3f(0);
                break;
            case KEYBOARD_PRESSED:
                debug::print("Key pressed: %d\n", event.keycode);
                if (keyboard_bindings.find(event.keycode) != keyboard_bindings.end())
                {
                    buttons_down |= keyboard_bindings[event.keycode];
                    buttons_held |= keyboard_bindings[event.keycode];
                }
                break;
            case KEYBOARD_RELEASED:
                debug::print("Key released: %d\n", event.keycode);
                if (keyboard_bindings.find(event.keycode) != keyboard_bindings.end())
                {
                    buttons_held &= ~keyboard_bindings[event.keycode];
                }
                break;
            }
        }
        // Get mouse events
        if (MOUSE_IsConnected())
        {
            mouse_event event;
            while (MOUSE_GetEvent(&event))
            {
                // debug::print("Mouse event: button=0x%02x, rx=%d, ry=%d, rz=%d\n", event.button, event.rx, event.ry, event.rz);
                event.button &= 0x07; // Mask to only keep the first 3 bits for button states

                // If scroll wheel is used, we treat it as a button
                if (event.rz > 0)
                {
                    event.button |= MOUSE_BUTTON_WHEEL_UP;
                }
                else if (event.rz < 0)
                {
                    event.button |= MOUSE_BUTTON_WHEEL_DOWN;
                }

                // Process mouse buttons
                for (uint8_t i = 0; i < 5; i++)
                {
                    if (event.button & (1 << i))
                    {
                        if (mouse_button_bindings.find(1 << i) != mouse_button_bindings.end())
                        {
                            buttons_down |= mouse_button_bindings[1 << i];
                            buttons_held |= mouse_button_bindings[1 << i];
                        }
                    }
                    else
                    {
                        if (mouse_button_bindings.find(1 << i) != mouse_button_bindings.end())
                        {
                            buttons_held &= ~mouse_button_bindings[1 << i];
                        }
                    }
                }

                // Process mouse movement
                vec3f mouse_movement(event.rx / 64.0f, event.ry / -64.0f, event.rz);
                for (const auto &pair : joystick_bindings)
                {
                    if (pair.second == joystick_src_mouse_move)
                    {
                        joysticks[pair.first] = joysticks[pair.first] + mouse_movement;
                    }
                    else if (pair.second == joystick_src_mouse_scroll)
                    {
                        // Use the mouse scroll as a joystick input
                        joysticks[pair.first].z += mouse_movement.z;
                    }
                }
            }
        }

        // Handle joystick emulation
        joysticks[JOY_LEFT].x += bool(buttons_held & JOYLEFT_RIGHT);
        joysticks[JOY_LEFT].x -= bool(buttons_held & JOYLEFT_LEFT);
        joysticks[JOY_LEFT].y += bool(buttons_held & JOYLEFT_UP);
        joysticks[JOY_LEFT].y -= bool(buttons_held & JOYLEFT_DOWN);
        joysticks[JOY_RIGHT].x += bool(buttons_held & JOYRIGHT_RIGHT);
        joysticks[JOY_RIGHT].x -= bool(buttons_held & JOYRIGHT_LEFT);
        joysticks[JOY_RIGHT].y += bool(buttons_held & JOYRIGHT_DOWN);
        joysticks[JOY_RIGHT].y -= bool(buttons_held & JOYRIGHT_UP);
    }

    bool keyboard_mouse::connected() const
    {
        return keyboard_connected || MOUSE_IsConnected();
    }
}