#ifndef WIIMOTE_NUNCHUK_HPP
#define WIIMOTE_NUNCHUK_HPP
#include "input.hpp"

namespace input
{
    class wiimote_nunchuk : public device
    {
    public:
        wiimote_nunchuk();
        ~wiimote_nunchuk() override;

        void scan() override;
        float get_pointer_x() const override { return pointer_position.x; }
        float get_pointer_y() const override { return pointer_position.y; }
        bool connected() const override;
    };
}

#endif