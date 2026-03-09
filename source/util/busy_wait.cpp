#include "busy_wait.hpp"

#include <ogc/lwp.h>
#include <gctypes.h>
#include <stdexcept>
#include <util/lock.hpp>
#include <util/debuglog.hpp>

extern void *frameBuffer[2];
static lwp_t thread_handle = LWP_THREAD_NULL;
static int fb = 0;
static s8 thread_done = 0;
static std::function<void()> active_blocking_call = nullptr;

static void draw_busy_gui()
{
    GX_InvalidateTexAll();
    GX_InvVtxCache();
    GX_Flush();

    // Use 0 fractional bits for the position data, because we're drawing in pixel space
    gertex::set_pos_precision(GX_S16, 0);
    // Enable direct colors
    gertex::set_color_format(0, GX_DIRECT);
    // Turn alpha updates on
    GX_SetAlphaUpdate(GX_TRUE);
    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    // Disable fog
    gertex::use_fog(false);
    // Use normal blending
    gertex::set_blending(gertex::GXBlendMode::normal);

    gertex::ortho(gertex::get_state().view);

    // Draw the GUI elements
    Gui::get_gui()->draw();

    GX_SetDrawDone();

    GX_CopyDisp(frameBuffer[fb], GX_TRUE);
    GX_Flush();
    GX_PixModeSync();
#ifdef DEBUG
    debug::copy_to(frameBuffer[fb]);
#endif
    VIDEO_SetNextFramebuffer(frameBuffer[fb]);
    VIDEO_Flush();

    // Swap buffers
    fb ^= 1;
}

void busy_wait(std::function<void()> blocking_call, Gui *gui)
{
    if (thread_handle != LWP_THREAD_NULL)
        throw std::runtime_error("Attempt to enter busy wait twice");
    if (!blocking_call)
        return;
    active_blocking_call = blocking_call;
    Gui::set_gui(gui);

    thread_done = 0;

    auto wrapper = [](void *) -> void *
    {
        active_blocking_call();
        thread_done = 1;
        return NULL;
    };

    GX_InvalidateTexAll();
    GX_InvVtxCache();
    GX_Flush();
    // Use a lower priority (higher number) for the worker thread so the main thread can update the screen
    LWP_CreateThread(&thread_handle, wrapper, NULL, NULL, 0, 80);

    while (!thread_done)
    {
        draw_busy_gui();
        VIDEO_WaitVSync();
    }

    Gui::set_gui(nullptr);
    LWP_JoinThread(thread_handle, NULL);
    thread_handle = LWP_THREAD_NULL;
}