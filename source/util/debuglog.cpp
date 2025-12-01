#include "debuglog.hpp"
#include <malloc.h>

namespace debug
{
    static bool initialized = false;
    static GXRModeObj *rmode = NULL;
    static void *frame_buffer = NULL;

    void init(GXRModeObj *mode)
    {
        // We need the rmode to be set before we can allocate the framebuffer
        rmode = mode ? mode : VIDEO_GetPreferredMode(NULL);

        // Allocate the framebuffer
        frame_buffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
        if (!frame_buffer)
            return;

        GX_SetScissor(0, 0, rmode->fbWidth - 224, rmode->efbHeight);
        GX_SetDispCopySrc(0, 0, rmode->fbWidth - 224, rmode->efbHeight);

        // Initialize the console with the framebuffer and video mode
        CON_Init(frame_buffer, 0, 0, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

        // All done.
        initialized = true;
    }

    void print(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        // Immediately display the output

        // Ensure that the framebuffer is available
        void *fb = VIDEO_GetNextFramebuffer();

        if (!fb || !rmode)
        {
            // If the framebuffer is not available, we cannot display the debug output.
            // Hopefully the framebuffer will be available soon.
            return;
        }

        copy_to(fb);
        VIDEO_Flush();
    }

    void copy_to(void *fb)
    {
        if (!initialized || !fb)
            return;

        for (int i = 0; i < rmode->efbHeight; i++)
        {
            memcpy((char *)fb + i * rmode->viWidth * VI_DISPLAY_PIX_SZ + (rmode->viWidth - 224) * VI_DISPLAY_PIX_SZ, (char *)frame_buffer + i * rmode->viWidth * VI_DISPLAY_PIX_SZ, 224 * VI_DISPLAY_PIX_SZ);
        }
    }

    void deinit()
    {
        if (!initialized)
            return;

        // Free the framebuffer
        if (frame_buffer)
        {
            free(frame_buffer);
            frame_buffer = NULL;
        }

        // Reset the initialized flag
        initialized = false;
    }
}
#ifdef DEBUG

// Gotta hate crashes sending me to Wii Menu when debugging.
void abort()
{
    CON_Init(VIDEO_GetCurrentFramebuffer(), 0, 0, 640, 480, 1280);
    printf("\n\nThe game has encountered an unexpected error.\nYou can:\n- Hold power button of your console to shut down.\n- Debug the crash in Dolphin Emulator.");
    VIDEO_SetNextFramebuffer(VIDEO_GetCurrentFramebuffer());
    VIDEO_Flush();
    VIDEO_WaitVSync();
    VIDEO_WaitVSync();
    while (1)
        ;
}
#endif