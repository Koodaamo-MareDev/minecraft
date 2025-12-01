#ifndef BUSY_WAIT_CPP
#define BUSY_WAIT_CPP

#include <functional>
#include <gui/gui.hpp>

// Calls a blocking function while displaying a Gui
void busy_wait(std::function<void()> blocking_call, Gui *gui = nullptr);

#endif