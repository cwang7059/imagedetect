/* Minimal Snap7 C API declarations for runtime LoadLibrary (LGPL Snap7). */
#pragma once

#include <stdint.h>

#ifdef _WIN32
#  define S7API __stdcall
#else
#  define S7API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t S7Object;

typedef S7Object (S7API *Cli_Create_t)(void);
typedef int (S7API *Cli_Destroy_t)(S7Object *client);
typedef int (S7API *Cli_ConnectTo_t)(S7Object client, const char *address, int rack, int slot);
typedef int (S7API *Cli_Disconnect_t)(S7Object client);
typedef int (S7API *Cli_DBRead_t)(S7Object client, int dbNumber, int start, int size, void *usrData);
typedef int (S7API *Cli_DBWrite_t)(S7Object client, int dbNumber, int start, int size, void *usrData);

#ifdef __cplusplus
}
#endif
