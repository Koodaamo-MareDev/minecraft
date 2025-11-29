#ifndef BUSY_WAIT_CPP
#define BUSY_WAIT_CPP

#include <functional>
#include <string>

void busy_wait(std::function<void()> blocking_call, std::string description = "");

#endif