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

#ifndef SDL_config_purec_h_
#define SDL_config_purec_h_
#define SDL_config_h_

#include "SDL_platform.h"

/* PureC OS: freestanding gcc headers plus the small PureC hosted libc. */
#define HAVE_STDARG_H   1
#define HAVE_STDDEF_H   1
#define HAVE_STDINT_H   1
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1

#define HAVE_LIBC       1
#define HAVE_STDLIB_H   1
#define HAVE_STRING_H   1
#define HAVE_MALLOC     1
#define HAVE_CALLOC     1
#define HAVE_REALLOC    1
#define HAVE_FREE       1
#define HAVE_MEMSET     1
#define HAVE_MEMCPY     1
#define HAVE_MEMMOVE    1
#define HAVE_MEMCMP     1

/* No threads, no dynamic loading, no extra input devices. */
#define SDL_THREADS_DISABLED    1
#define SDL_LOADSO_DISABLED     1
#define SDL_JOYSTICK_DISABLED   1
#define SDL_HAPTIC_DISABLED     1
#define SDL_HIDAPI_DISABLED     1
#define SDL_SENSOR_DISABLED     1
#define SDL_TIMERS_DISABLED     1

#define SDL_TIMER_PUREC         1
#define SDL_AUDIO_DRIVER_PUREC  1
#define SDL_AUDIO_DRIVER_DUMMY  1
#define SDL_VIDEO_DRIVER_PUREC  1
#define SDL_FILESYSTEM_DUMMY    1

#define SDL_VIDEO_RENDER_SW     1

#endif /* SDL_config_purec_h_ */
