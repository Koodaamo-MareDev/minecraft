#ifndef INPUT_HPP
#define INPUT_HPP

#include <gctypes.h>
#include <wiikeyboard/usbkeyboard.h>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "../math/vec3f.hpp"
#include "../config.hpp"

namespace input
{
    constexpr uint8_t JOY_LEFT = 0;
    constexpr uint8_t JOY_RIGHT = 1;
    constexpr uint8_t JOY_AUX = 2;

    constexpr uint32_t BUTTON_CONFIRM = 0x0001;
    constexpr uint32_t BUTTON_JUMP = 0x0002;
    constexpr uint32_t BUTTON_INVENTORY = 0x0004;
    constexpr uint32_t BUTTON_CANCEL = 0x0008;
    constexpr uint32_t BUTTON_HOTBAR_LEFT = 0x0010;
    constexpr uint32_t BUTTON_HOTBAR_RIGHT = 0x0020;
    constexpr uint32_t BUTTON_MINE = 0x0040;
    constexpr uint32_t BUTTON_PLACE = 0x0080;
    constexpr uint32_t BUTTON_TOGGLE_SNEAK = 0x0100;
    constexpr uint32_t BUTTON_TOGGLE_PAUSE = 0x0200;

    constexpr uint32_t JOYLEFT_UP = 0x00010000;
    constexpr uint32_t JOYLEFT_DOWN = 0x00020000;
    constexpr uint32_t JOYLEFT_LEFT = 0x00040000;
    constexpr uint32_t JOYLEFT_RIGHT = 0x00080000;

    constexpr uint32_t JOYRIGHT_UP = 0x00100000;
    constexpr uint32_t JOYRIGHT_DOWN = 0x00200000;
    constexpr uint32_t JOYRIGHT_LEFT = 0x00400000;
    constexpr uint32_t JOYRIGHT_RIGHT = 0x00800000;

    constexpr uint32_t JOYAUX_UP = 0x01000000;
    constexpr uint32_t JOYAUX_DOWN = 0x02000000;
    constexpr uint32_t JOYAUX_LEFT = 0x04000000;
    constexpr uint32_t JOYAUX_RIGHT = 0x08000000;

    class device
    {
    public:
        virtual ~device() = default;

        virtual void scan() = 0;
        virtual uint32_t get_buttons_down() const { return buttons_down; }
        virtual uint32_t get_buttons_held() const { return buttons_held; }
        virtual vec3f get_left_stick() const { return joysticks[JOY_LEFT]; }
        virtual vec3f get_right_stick() const { return joysticks[JOY_RIGHT]; }
        virtual vec3f get_aux_stick() const { return joysticks[JOY_AUX]; }
        virtual bool is_ir_visible() const { return pointer_visible; }
        virtual float get_pointer_x() const { return 0.5f; }
        virtual float get_pointer_y() const { return 0.5f; }
        virtual bool connected() const { return false; } // Returns true if the device is connected, false otherwise

    protected:
        uint32_t buttons_held = 0;
        uint32_t buttons_down = 0;
        vec3f joysticks[3] = {vec3f(0), vec3f(0), vec3f(0)};
        vec3f pointer_position = vec3f(0, 0, 0);
        bool pointer_visible = false;
    };

    extern std::map<std::string, uint8_t> string_joystick_src_mapping;
    extern std::map<std::string, uint8_t> string_joystick_dst_mapping;
    extern std::map<std::string, uint32_t> string_button_mapping;
    extern std::vector<input::device *> devices;
    extern float deadzone;
    extern float sensitivity;
    extern configuration config;

    void init();
    void deinit();
    void reload_config();
    void add_device(device *dev);
}

#endif