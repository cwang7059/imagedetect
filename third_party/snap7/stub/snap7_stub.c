/*
 * Minimal snap7.dll stub for offline/dev runs.
 * In-memory DB store; exports Cli_* used by Snap7Loader.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t S7Object;

typedef struct {
    int used;
    unsigned char mem[256][4096]; /* db -> bytes */
} S7StubState;

static S7StubState g_state;

__declspec(dllexport) S7Object __stdcall Cli_Create(void)
{
    S7StubState *s = (S7StubState *)calloc(1, sizeof(S7StubState));
    if (!s)
        return 0;
    s->used = 1;
    /* SysReady bit in DB100 offset 0 bit1 for point map compatibility if used */
    return (S7Object)s;
}

__declspec(dllexport) int __stdcall Cli_Destroy(S7Object *client)
{
    if (!client || !*client)
        return 0;
    free((void *)*client);
    *client = 0;
    return 0;
}

__declspec(dllexport) int __stdcall Cli_ConnectTo(S7Object client, const char *address, int rack, int slot)
{
    S7StubState *s = (S7StubState *)client;
    (void)address;
    (void)rack;
    (void)slot;
    if (!s)
        return -1;
    /* DB100 byte0 bit1 = SysReady (matches config/plc_points.json) */
    s->mem[100][0] = (unsigned char)(s->mem[100][0] | (1u << 1));
    return 0;
}

__declspec(dllexport) int __stdcall Cli_Disconnect(S7Object client)
{
    (void)client;
    return 0;
}

__declspec(dllexport) int __stdcall Cli_DBRead(S7Object client, int dbNumber, int start, int size, void *usrData)
{
    S7StubState *s = (S7StubState *)client;
    if (!s || !usrData || size < 0 || start < 0)
        return -1;
    if (dbNumber < 0 || dbNumber >= 256)
        return -1;
    if (start + size > 4096)
        return -1;
    memcpy(usrData, s->mem[dbNumber] + start, (size_t)size);
    return 0;
}

__declspec(dllexport) int __stdcall Cli_DBWrite(S7Object client, int dbNumber, int start, int size, void *usrData)
{
    S7StubState *s = (S7StubState *)client;
    if (!s || !usrData || size < 0 || start < 0)
        return -1;
    if (dbNumber < 0 || dbNumber >= 256)
        return -1;
    if (start + size > 4096)
        return -1;
    memcpy(s->mem[dbNumber] + start, usrData, (size_t)size);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h;
    (void)reason;
    (void)r;
    (void)g_state;
    return TRUE;
}

#ifdef __cplusplus
}
#endif
