#include "wiimote_nunchuk.hpp"

#define MAP_BUTTON(_from, _to, _fromstate, _tostate) _tostate = ((_tostate) & ~(_from)) | (bool((_fromstate) & (_from)) * (_to))

#include <wiiuse/wpad.h>

namespace input
{
    wiimote_nunchuk::wiimote_nunchuk()
    {
        WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    }

    wiimote_nunchuk::~wiimote_nunchuk()
    {
    }

    void wiimote_nunchuk::scan()
    {
        WPADData *data = WPAD_Data(0);
        if (data->exp.type == WPAD_EXP_NUNCHUK)
        {
            buttons_down = data->btns_d;
            buttons_held = data->btns_h;

            joysticks[JOY_LEFT].x = float(int(data->exp.nunchuk.js.pos.x) - int(data->exp.nunchuk.js.center.x)) * 2 / (int(data->exp.nunchuk.js.max.x) - 2 - int(data->exp.nunchuk.js.min.x));
            joysticks[JOY_LEFT].y = float(int(data->exp.nunchuk.js.pos.y) - int(data->exp.nunchuk.js.center.y)) * 2 / (int(data->exp.nunchuk.js.max.y) - 2 - int(data->exp.nunchuk.js.min.y));

            // Apply deadzone
            if (joysticks[JOY_LEFT].magnitude() < 0.1f)
            {
                joysticks[JOY_LEFT].x = 0;
                joysticks[JOY_LEFT].y = 0;
            }

            // Map the buttons
            MAP_BUTTON(NUNCHUK_BUTTON_C, BUTTON_INVENTORY, data->exp.nunchuk.btns_held, buttons_held);
            MAP_BUTTON(NUNCHUK_BUTTON_Z, BUTTON_PLACE, data->exp.nunchuk.btns_held, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_A, BUTTON_JUMP, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_B, BUTTON_MINE, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_DOWN, BUTTON_TOGGLE_SNEAK, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_2, BUTTON_TOGGLE_PAUSE, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_MINUS, BUTTON_HOTBAR_LEFT, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_PLUS, BUTTON_HOTBAR_RIGHT, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_A, BUTTON_CONFIRM, data->btns_h, buttons_held);
            MAP_BUTTON(WPAD_BUTTON_B, BUTTON_CANCEL, data->btns_h, buttons_held);

            // Handle IR pointer
            if (data->ir.valid)
            {
                pointer_visible = true;

                pointer_position.x = data->ir.x / data->ir.vres[0];
                pointer_position.y = data->ir.y / data->ir.vres[1];

                joysticks[JOY_RIGHT].x = (data->ir.x - data->ir.vres[0]) - 0.5f;
                joysticks[JOY_RIGHT].y = 0.5 - (data->ir.y - data->ir.vres[1]);

                if (joysticks[JOY_RIGHT].magnitude() < 0.125)
                {
                    // Have a deadzone of 0.125 to avoid accidental movements
                    joysticks[JOY_RIGHT].x = 0;
                    joysticks[JOY_RIGHT].y = 0;
                }
                else
                {
                    // Magnitude of 0.25 or less means that the stick is near the center. 1 means it's at the edge and means it will move at full speed.
                    joysticks[JOY_RIGHT] = joysticks[JOY_RIGHT].fast_normalize() * ((joysticks[JOY_RIGHT].magnitude() - 0.125) / 0.875);

                    // Multiply the result by 2 since the edges of the screen are difficult to reach as tracking is lost.
                    joysticks[JOY_RIGHT] = joysticks[JOY_RIGHT] * 2;
                }
            }
            else
            {
                pointer_visible = false;
            }

            if (joysticks[JOY_RIGHT].magnitude() > 1.0)
                joysticks[JOY_RIGHT] = joysticks[JOY_RIGHT].fast_normalize();
        }
    }

    bool wiimote_nunchuk::connected() const
    {
        return WPAD_Data(0)->exp.type == WPAD_EXP_NUNCHUK;
    }

} // namespace input