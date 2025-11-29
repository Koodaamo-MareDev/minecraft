#include "input_devices.hpp"

namespace registry
{
    void register_input_devices()
    {
        input::add_device(new input::KeyboardMouse);
        input::add_device(new input::WiimoteNunchuk);
        input::add_device(new input::WiimoteClassic);
    }
} // namespace registry
