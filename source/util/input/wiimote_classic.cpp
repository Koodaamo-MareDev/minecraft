#include "wiimote_classic.hpp"
#include <wiiuse/wpad.h>
#include "input.hpp"

#define MAP_BUTTON(_from, _to, _fromstate, _tostate) \
    if ((_fromstate) & (_from))                      \
    {                                                \
        (_tostate) |= (_to);                         \
    }                                                \
    else                                             \
    {                                                \
        (_tostate) &= ~(_to);                        \
    }
namespace input
{
    WiimoteClassic::WiimoteClassic()
    {
    }

    WiimoteClassic::~WiimoteClassic()
    {
    }

    void WiimoteClassic::scan()
    {
        WPADData *data = WPAD_Data(0);
        if (data->exp.type == WPAD_EXP_CLASSIC)
        {
            joysticks[JOY_LEFT].x = float(int(data->exp.classic.ljs.pos.x) - int(data->exp.classic.ljs.center.x)) * 2 / (int(data->exp.classic.ljs.max.x) - 2 - int(data->exp.classic.ljs.min.x));
            joysticks[JOY_LEFT].y = float(int(data->exp.classic.ljs.pos.y) - int(data->exp.classic.ljs.center.y)) * 2 / (int(data->exp.classic.ljs.max.y) - 2 - int(data->exp.classic.ljs.min.y));

            joysticks[JOY_RIGHT].x = float(int(data->exp.classic.rjs.pos.x) - int(data->exp.classic.rjs.center.x)) * 2 / (int(data->exp.classic.rjs.max.x) - 2 - int(data->exp.classic.rjs.min.x));
            joysticks[JOY_RIGHT].y = float(int(data->exp.classic.rjs.pos.y) - int(data->exp.classic.rjs.center.y)) * 2 / (int(data->exp.classic.rjs.max.y) - 2 - int(data->exp.classic.rjs.min.y));

            // Apply deadzone
            if (joysticks[JOY_LEFT].magnitude() < 0.1f)
            {
                joysticks[JOY_LEFT].x = 0;
                joysticks[JOY_LEFT].y = 0;
            }
            WPAD_Expansion(0, &data->exp);
            uint32_t classic_btn_h = WPAD_ButtonsHeld(0);
            uint32_t classic_btn_d = WPAD_ButtonsDown(0);

            MAP_BUTTON(WPAD_CLASSIC_BUTTON_DOWN, BUTTON_TOGGLE_SNEAK, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_LEFT, BUTTON_HOTBAR_LEFT, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_RIGHT, BUTTON_HOTBAR_RIGHT, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_A, BUTTON_CONFIRM, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_B, BUTTON_CANCEL, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_B, BUTTON_JUMP, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_X, BUTTON_INVENTORY, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_ZL, BUTTON_HOTBAR_LEFT, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_ZR, BUTTON_HOTBAR_RIGHT, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_FULL_L, BUTTON_PLACE, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_FULL_R, BUTTON_MINE, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_MINUS, BUTTON_TOGGLE_PAUSE, classic_btn_h, buttons_held);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_PLUS, BUTTON_TOGGLE_PAUSE, classic_btn_h, buttons_held);

            MAP_BUTTON(WPAD_CLASSIC_BUTTON_DOWN, BUTTON_TOGGLE_SNEAK, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_LEFT, BUTTON_HOTBAR_LEFT, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_RIGHT, BUTTON_HOTBAR_RIGHT, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_A, BUTTON_CONFIRM, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_B, BUTTON_CANCEL, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_B, BUTTON_JUMP, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_X, BUTTON_INVENTORY, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_ZL, BUTTON_HOTBAR_LEFT, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_ZR, BUTTON_HOTBAR_RIGHT, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_FULL_L, BUTTON_PLACE, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_FULL_R, BUTTON_MINE, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_MINUS, BUTTON_TOGGLE_PAUSE, classic_btn_d, buttons_down);
            MAP_BUTTON(WPAD_CLASSIC_BUTTON_PLUS, BUTTON_TOGGLE_PAUSE, classic_btn_d, buttons_down);

            if (joysticks[JOY_RIGHT].sqr_magnitude() > 1.0)
                joysticks[JOY_RIGHT] = joysticks[JOY_RIGHT].fast_normalize();
        }
    }
}

bool input::WiimoteClassic::connected() const
{
    return WPAD_Data(0)->exp.type == WPAD_EXP_CLASSIC;
}
