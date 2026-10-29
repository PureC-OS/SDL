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

#ifdef SDL_AUDIO_DRIVER_PUREC

/* PureC OS audio driver on top of the PureAudio PCM stream (22050 Hz, mono,
   signed 16-bit). SDL converts whatever the application asks for to that
   format; the callback runs from PUREC_PumpAudio(). */

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_purecaudio.h"

#include "libaudio/include/pureaudio.h"

#define PUREC_AUDIO_RATE 22050
#define PUREC_RING_BYTES 65536

static SDL_AudioDevice *purec_device;
static Uint8 purec_ring[PUREC_RING_BYTES];
static size_t purec_ring_head;
static size_t purec_ring_tail;
static size_t purec_ring_count;

static void ring_put(const Uint8 *data, size_t length)
{
    size_t i;
    for (i = 0; i < length && purec_ring_count < PUREC_RING_BYTES; ++i) {
        purec_ring[purec_ring_head] = data[i];
        purec_ring_head = (purec_ring_head + 1) % PUREC_RING_BYTES;
        ++purec_ring_count;
    }
}

static void ring_get(Uint8 *out, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        if (purec_ring_count == 0) {
            out[i] = 0;
            continue;
        }
        out[i] = purec_ring[purec_ring_tail];
        purec_ring_tail = (purec_ring_tail + 1) % PUREC_RING_BYTES;
        --purec_ring_count;
    }
}

static void PUREC_ProduceChunk(SDL_AudioDevice *device)
{
    Uint8 *data = device->work_buffer;
    int data_len = device->callbackspec.size;

    SDL_LockMutex(device->mixer_lock);
    if (SDL_AtomicGet(&device->paused)) {
        SDL_memset(data, device->callbackspec.silence, data_len);
    } else {
        device->callbackspec.callback(device->callbackspec.userdata, data, data_len);
    }
    SDL_UnlockMutex(device->mixer_lock);

    if (device->stream) {
        Uint8 converted[4096];
        int got;
        SDL_AudioStreamPut(device->stream, data, data_len);
        while ((got = SDL_AudioStreamGet(device->stream, converted, sizeof(converted))) > 0) {
            ring_put(converted, (size_t)got);
        }
    } else {
        ring_put(data, (size_t)data_len);
    }
}

static void PUREC_Fill(Sint16 *frames, Uint32 count, void *user)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)user;
    size_t wanted = (size_t)count * sizeof(Sint16);
    int guard = 64;

    while (purec_ring_count < wanted && guard-- > 0) {
        PUREC_ProduceChunk(device);
    }
    ring_get((Uint8 *)frames, wanted);
}

void PUREC_PumpAudio(void)
{
    if (purec_device && pa_stream_is_open()) {
        pa_stream_pump();
    }
}

static int PUREC_OpenDevice(_THIS, const char *devname)
{
    if (purec_device) {
        return SDL_SetError("PureC audio supports one playback device");
    }

    _this->spec.freq = PUREC_AUDIO_RATE;
    _this->spec.channels = 1;
    _this->spec.format = AUDIO_S16SYS;
    SDL_CalculateAudioSpec(&_this->spec);

    purec_ring_head = purec_ring_tail = purec_ring_count = 0;
    if (pa_stream_open((pa_stream_fill_fn)PUREC_Fill, _this) != 0) {
        return SDL_SetError("PureAudio cannot open a PCM stream");
    }
    purec_device = _this;
    _this->hidden = (void *)0x1;
    return 0;
}

static void PUREC_CloseDevice(_THIS)
{
    if (purec_device == _this) {
        pa_stream_close();
        purec_device = NULL;
    }
}

static void PUREC_WaitDevice(_THIS)
{
}

static void PUREC_PlayDevice(_THIS)
{
}

static Uint8 *PUREC_GetDeviceBuf(_THIS)
{
    return NULL;
}

static SDL_bool PUREC_Init(SDL_AudioDriverImpl *impl)
{
    struct pa_status status;
    if (pa_get_status(&status) != 0 || !status.pcm_ready) {
        return SDL_FALSE;
    }

    impl->OpenDevice = PUREC_OpenDevice;
    impl->CloseDevice = PUREC_CloseDevice;
    impl->WaitDevice = PUREC_WaitDevice;
    impl->PlayDevice = PUREC_PlayDevice;
    impl->GetDeviceBuf = PUREC_GetDeviceBuf;
    impl->ProvidesOwnCallbackThread = SDL_TRUE;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    return SDL_TRUE;
}

AudioBootStrap PUREC_AUDIO_bootstrap = {
    "purec", "PureC OS PureAudio", PUREC_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_PUREC */

/* vi: set ts=4 sw=4 expandtab: */
