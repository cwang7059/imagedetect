/*
 * OnnaCamera.dll stub: synthetic line-scan BGR frames for dry-run without vendor SDK.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ONNA_CAMERA_EXPORTS
#include "../include/onna_camera_c_api.h"

typedef struct {
    int triggerMode;
    int running;
    int w;
    int h;
} OnnaCtx;

__declspec(dllexport) int __stdcall Onna_Init(void)
{
    return 0;
}

__declspec(dllexport) void __stdcall Onna_Shutdown(void)
{
}

__declspec(dllexport) int __stdcall Onna_Open(OnnaHandle *out, int index, const char *serial)
{
    (void)index;
    (void)serial;
    if (!out)
        return -1;
    OnnaCtx *c = (OnnaCtx *)calloc(1, sizeof(OnnaCtx));
    if (!c)
        return -2;
    c->w = 640;
    c->h = 240;
    c->triggerMode = 1;
    *out = c;
    return 0;
}

__declspec(dllexport) void __stdcall Onna_Close(OnnaHandle h)
{
    free(h);
}

__declspec(dllexport) int __stdcall Onna_SetTriggerMode(OnnaHandle h, int mode)
{
    OnnaCtx *c = (OnnaCtx *)h;
    if (!c)
        return -1;
    c->triggerMode = mode;
    return 0;
}

__declspec(dllexport) int __stdcall Onna_Start(OnnaHandle h)
{
    OnnaCtx *c = (OnnaCtx *)h;
    if (!c)
        return -1;
    c->running = 1;
    return 0;
}

__declspec(dllexport) int __stdcall Onna_Stop(OnnaHandle h)
{
    OnnaCtx *c = (OnnaCtx *)h;
    if (!c)
        return -1;
    c->running = 0;
    return 0;
}

__declspec(dllexport) int __stdcall Onna_GrabBgr(OnnaHandle h,
                                                 unsigned char *buf,
                                                 int bufSize,
                                                 OnnaFrameInfo *info,
                                                 int timeoutMs)
{
    OnnaCtx *c = (OnnaCtx *)h;
    int x, y, need;
    (void)timeoutMs;
    if (!c || !c->running || !buf || !info)
        return -1;
    need = c->w * c->h * 3;
    if (bufSize < need)
        return -2;
    memset(buf, 30, (size_t)need);
    /* bright horizontal cylinder band */
    for (y = c->h / 2 - 30; y <= c->h / 2 + 30; ++y) {
        if (y < 0 || y >= c->h)
            continue;
        for (x = c->w / 8; x < c->w - c->w / 8; ++x) {
            unsigned char *p = buf + (y * c->w + x) * 3;
            p[0] = p[1] = p[2] = 200;
        }
    }
    info->width = c->w;
    info->height = c->h;
    info->channels = 3;
    info->payloadBytes = need;
    return 0;
}

__declspec(dllexport) int __stdcall Onna_RegisterCallback(OnnaHandle h, OnnaFrameCallback cb, void *user)
{
    (void)h;
    (void)cb;
    (void)user;
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hi, DWORD reason, LPVOID r)
{
    (void)hi;
    (void)reason;
    (void)r;
    return TRUE;
}
