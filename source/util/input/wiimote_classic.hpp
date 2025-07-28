#ifndef WIIMOTE_CLASSIC_HPP
#define WIIMOTE_CLASSIC_HPP

#include "input.hpp"

namespace input
{
    class wiimote_classic : public device
    {
    public:
        wiimote_classic();
        ~wiimote_classic() override;

        void scan() override;
        bool connected() const override;
    };
}

#endif