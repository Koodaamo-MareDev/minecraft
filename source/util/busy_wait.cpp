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
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    // Turn alpha updates on
    GX_SetAlphaUpdate(GX_TRUE);
    // Disable depth testing for GUI elements
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    // Disable fog
    gertex::use_fog(false);
    // Use normal blending
    gertex::set_blending(gertex::GXBlendMode::normal);

    // Reset the orthogonal position matrix and push it onto the stack
    gertex::ortho(gertex::get_state().view);
    gertex::push_matrix();

    // Draw the GUI elements
    Gui::get_gui()->draw();

    // Restore the orthogonal position matrix
    gertex::pop_matrix();
    gertex::load_pos_matrix();
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