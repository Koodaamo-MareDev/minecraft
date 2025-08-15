#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

#include <gctypes.h>
#include <wiikeyboard/keyboard.h>
#include <ogc/usbmouse.h>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>
#include "input.hpp"
#include "../config.hpp"
namespace input
{
    constexpr uint8_t MOUSE_BUTTON_LEFT = 0x01;
    constexpr uint8_t MOUSE_BUTTON_RIGHT = 0x02;
    constexpr uint8_t MOUSE_BUTTON_MIDDLE = 0x04;
    constexpr uint8_t MOUSE_BUTTON_WHEEL_UP = 0x08;
    constexpr uint8_t MOUSE_BUTTON_WHEEL_DOWN = 0x10;

    class KeyboardMouse : public Device
    {
    public:
        KeyboardMouse();
        ~KeyboardMouse() override;
        void scan() override;
        bool connected() const override;
        std::map<uint8_t, uint8_t> joystick_bindings;
        std::map<uint8_t, uint32_t> keyboard_bindings;
        std::map<uint8_t, uint32_t> mouse_button_bindings;

    private:
        static void init();
        bool keyboard_connected = false;
    };
}
#endif