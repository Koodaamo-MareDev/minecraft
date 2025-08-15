#ifndef WIIMOTE_CLASSIC_HPP
#define WIIMOTE_CLASSIC_HPP

#include "input.hpp"

namespace input
{
    class WiimoteClassic : public Device
    {
    public:
        WiimoteClassic();
        ~WiimoteClassic() override;

        void scan() override;
        bool connected() const override;
    };
}

#endif