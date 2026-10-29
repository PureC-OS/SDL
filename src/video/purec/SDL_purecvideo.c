/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PUREC

/* PureC OS video driver: every SDL window is a PureGUI window, the
   software framebuffer is pushed to the compositor with pg_window_blit(). */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_purecvideo.h"

#include "libc/include/purec.h"
#include "libgui/include/puregui.h"

#define PUREC_DRIVER_NAME  "purec"
#define PUREC_SURFACE      "_SDL_PureCSurface"
#define PUREC_FRAME_WIDTH  (PG_WINDOW_BORDER * 2)
#define PUREC_FRAME_HEIGHT (PG_TITLEBAR_HEIGHT + PG_WINDOW_BORDER * 2)
#define PUREC_MAX_EVENTS   256

typedef struct
{
    struct pg_window window;
    SDL_bool mouse_inside;
} SDL_WindowData;

static SDL_Window *purec_window;

static int PUREC_Available(void)
{
    return 1;
}

static void PUREC_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static void PUREC_Present(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    SDL_Surface *surface = (SDL_Surface *)SDL_GetWindowData(window, PUREC_SURFACE);
    if (!data || !surface) {
        return;
    }
    pg_window_blit(&data->window, 0, 0, (uint32_t)surface->w, (uint32_t)surface->h,
                   (const uint32_t *)surface->pixels, (uint32_t)(surface->pitch / 4));
}

static void PUREC_Repaint(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    pg_window_begin(&data->window);
    pg_window_end(&data->window);
    PUREC_Present(window);
}

static int PUREC_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_RGB888;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    surface = (SDL_Surface *)SDL_SetWindowData(window, PUREC_SURFACE, surface);
    SDL_FreeSurface(surface);
    surface = (SDL_Surface *)SDL_GetWindowData(window, PUREC_SURFACE);

    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

static int PUREC_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    SDL_Surface *surface = (SDL_Surface *)SDL_GetWindowData(window, PUREC_SURFACE);
    int i;

    if (!data || !surface) {
        return SDL_SetError("Couldn't find PureC surface for window");
    }
    for (i = 0; i < numrects; ++i) {
        SDL_Rect r = rects[i];
        if (r.x < 0) {
            r.w += r.x;
            r.x = 0;
        }
        if (r.y < 0) {
            r.h += r.y;
            r.y = 0;
        }
        if (r.x + r.w > surface->w) {
            r.w = surface->w - r.x;
        }
        if (r.y + r.h > surface->h) {
            r.h = surface->h - r.y;
        }
        if (r.w <= 0 || r.h <= 0) {
            continue;
        }
        pg_window_blit(&data->window, r.x, r.y, (uint32_t)r.w, (uint32_t)r.h,
                       (const uint32_t *)((const Uint8 *)surface->pixels + r.y * surface->pitch + r.x * 4),
                       (uint32_t)(surface->pitch / 4));
    }
    return 0;
}

static void PUREC_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface = (SDL_Surface *)SDL_SetWindowData(window, PUREC_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

static int PUREC_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    struct pc_display_info display;
    uint32_t frame_w = (uint32_t)window->w + PUREC_FRAME_WIDTH;
    uint32_t frame_h = (uint32_t)window->h + PUREC_FRAME_HEIGHT;

    if (purec_window) {
        return SDL_SetError("PureC supports one SDL window per process");
    }
    if (!pc_display_get_info(&display) || !display.available) {
        return SDL_SetError("No display available");
    }
    if (window->w > (int)display.width - PUREC_FRAME_WIDTH) {
        window->w = (int)display.width - PUREC_FRAME_WIDTH;
    }
    if (window->h > (int)display.height - PUREC_FRAME_HEIGHT) {
        window->h = (int)display.height - PUREC_FRAME_HEIGHT;
    }
    frame_w = (uint32_t)window->w + PUREC_FRAME_WIDTH;
    frame_h = (uint32_t)window->h + PUREC_FRAME_HEIGHT;

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    if (!pg_window_center(&data->window, window->title, frame_w, frame_h)) {
        SDL_free(data);
        return SDL_SetError("Couldn't create a %ux%u PureGUI window", frame_w, frame_h);
    }
    pg_window_set_esc_closes(&data->window, false);

    window->driverdata = data;
    purec_window = window;
    window->flags |= SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS;

    PUREC_Repaint(window);
    SDL_SetKeyboardFocus(window);
    return 0;
}

static void PUREC_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return;
    }
    pg_window_close(&data->window);
    SDL_free(data);
    window->driverdata = NULL;
    if (purec_window == window) {
        purec_window = NULL;
    }
}

static void PUREC_SetWindowTitle(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return;
    }
    pc_copy(data->window.title, window->title ? window->title : "", sizeof(data->window.title));
    PUREC_Repaint(window);
}

static int PUREC_MapButton(uint8_t button)
{
    switch (button) {
    case 1:
        return SDL_BUTTON_LEFT;
    case 2:
        return SDL_BUTTON_RIGHT;
    case 3:
        return SDL_BUTTON_MIDDLE;
    default:
        return 0;
    }
}

static void PUREC_PumpEvents(_THIS)
{
    SDL_Window *window = purec_window;
    SDL_WindowData *data;
    struct pg_event ev;
    struct key_event key;
    int budget;
    int32_t wheel;

    if (!window || !window->driverdata) {
        return;
    }
    data = (SDL_WindowData *)window->driverdata;

    for (budget = PUREC_MAX_EVENTS; budget > 0 && pg_window_poll_key_event(&data->window, &key); --budget) {
        SDL_SendKeyboardKey(key.pressed ? SDL_PRESSED : SDL_RELEASED, (SDL_Scancode)key.scancode);
    }

    for (budget = PUREC_MAX_EVENTS; budget > 0 && pg_window_poll_event(&data->window, &ev); --budget) {
        int x = ev.x - (int)data->window.client.x;
        int y = ev.y - (int)data->window.client.y;
        SDL_bool inside = (x >= 0 && y >= 0 && x < window->w && y < window->h) ? SDL_TRUE : SDL_FALSE;

        switch (ev.type) {
        case PG_EVENT_CLOSE:
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_CLOSE, 0, 0);
            return;
        case PG_EVENT_KEY:
            if (ev.key >= 32 && ev.key < 127) {
                char text[2] = { (char)ev.key, '\0' };
                SDL_SendKeyboardText(text);
            }
            break;
        case PG_EVENT_FOCUS:
            SDL_SetKeyboardFocus(window);
            break;
        case PG_EVENT_REPAINT:
        case PG_EVENT_MOVE:
            PUREC_Repaint(window);
            break;
        case PG_EVENT_MINIMIZE:
            SDL_SendWindowEvent(window, data->window.minimized ? SDL_WINDOWEVENT_MINIMIZED : SDL_WINDOWEVENT_RESTORED, 0, 0);
            if (!data->window.minimized) {
                PUREC_Repaint(window);
            }
            break;
        case PG_EVENT_MOUSE_MOVE:
            if (inside && !data->mouse_inside) {
                SDL_SetMouseFocus(window);
            } else if (!inside && data->mouse_inside) {
                SDL_SetMouseFocus(NULL);
            }
            data->mouse_inside = inside;
            if (inside) {
                SDL_SendMouseMotion(window, 0, 0, x, y);
            }
            break;
        case PG_EVENT_MOUSE_DOWN:
        case PG_EVENT_MOUSE_UP:
            if (inside) {
                int button = PUREC_MapButton(ev.button);
                if (button) {
                    SDL_SendMouseButton(window, 0, ev.type == PG_EVENT_MOUSE_DOWN ? SDL_PRESSED : SDL_RELEASED, (Uint8)button);
                }
            }
            break;
        default:
            break;
        }
    }

    wheel = pg_window_take_wheel(&data->window);
    if (wheel && data->mouse_inside) {
        SDL_SendMouseWheel(window, 0, 0.0f, (float)wheel, SDL_MOUSEWHEEL_NORMAL);
    }
}

static int PUREC_VideoInit(_THIS)
{
    SDL_DisplayMode mode;
    struct pc_display_info display;
    SDL_Keycode keymap[SDL_NUM_SCANCODES];

    if (!pc_display_get_info(&display) || !display.available) {
        return SDL_SetError("No display available");
    }

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.w = (int)display.width;
    mode.h = (int)display.height;
    mode.refresh_rate = 60;
    mode.driverdata = NULL;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    SDL_AddDisplayMode(&_this->displays[0], &mode);

    SDL_GetDefaultKeymap(keymap);
    SDL_SetKeymap(0, keymap, SDL_NUM_SCANCODES, SDL_FALSE);
    return 0;
}

static void PUREC_VideoQuit(_THIS)
{
    purec_window = NULL;
}

static SDL_VideoDevice *PUREC_CreateDevice(void)
{
    SDL_VideoDevice *device;

    if (!PUREC_Available()) {
        return NULL;
    }
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->VideoInit = PUREC_VideoInit;
    device->VideoQuit = PUREC_VideoQuit;
    device->PumpEvents = PUREC_PumpEvents;
    device->CreateSDLWindow = PUREC_CreateWindow;
    device->DestroyWindow = PUREC_DestroyWindow;
    device->SetWindowTitle = PUREC_SetWindowTitle;
    device->CreateWindowFramebuffer = PUREC_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = PUREC_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = PUREC_DestroyWindowFramebuffer;
    device->free = PUREC_DeleteDevice;
    return device;
}

VideoBootStrap PUREC_bootstrap = {
    PUREC_DRIVER_NAME, "SDL PureC OS video driver",
    PUREC_CreateDevice,
    NULL /* no ShowMessageBox implementation */
};

#endif /* SDL_VIDEO_DRIVER_PUREC */

/* vi: set ts=4 sw=4 expandtab: */
