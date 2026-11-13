/**
 * Thin C ABI for Onna (昂纳) line-scan cameras.
 * Vendor SDK should be wrapped into OnnaCamera.dll exporting these symbols,
 * OR place a compatible GenICam wrapper DLL named OnnaCamera.dll.
 */
#pragma once

#ifdef _WIN32
#  ifdef ONNA_CAMERA_EXPORTS
#    define ONNA_API __declspec(dllexport)
#  else
#    define ONNA_API __declspec(dllimport)
#  endif
#  define ONNA_CALL __stdcall
#else
#  define ONNA_API
#  define ONNA_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *OnnaHandle;

typedef struct OnnaFrameInfo {
    int width;
    int height;
    int channels;   /* 1=gray 3=BGR */
    int payloadBytes;
} OnnaFrameInfo;

typedef void (ONNA_CALL *OnnaFrameCallback)(const unsigned char *data,
                                            const OnnaFrameInfo *info,
                                            void *user);

ONNA_API int ONNA_CALL Onna_Init(void);
ONNA_API void ONNA_CALL Onna_Shutdown(void);

/** Open by index (0..) or by serial string (serial may be NULL → index). */
ONNA_API int ONNA_CALL Onna_Open(OnnaHandle *out, int index, const char *serial);
ONNA_API void ONNA_CALL Onna_Close(OnnaHandle h);

/** 0=free-run, 1=hardware line trigger, 2=software trigger */
ONNA_API int ONNA_CALL Onna_SetTriggerMode(OnnaHandle h, int mode);
ONNA_API int ONNA_CALL Onna_Start(OnnaHandle h);
ONNA_API int ONNA_CALL Onna_Stop(OnnaHandle h);

/** Soft notify after PLC hard trigger: wait up to timeoutMs for one frame (BGR8 preferred). */
ONNA_API int ONNA_CALL Onna_GrabBgr(OnnaHandle h,
                                    unsigned char *buf,
                                    int bufSize,
                                    OnnaFrameInfo *info,
                                    int timeoutMs);

ONNA_API int ONNA_CALL Onna_RegisterCallback(OnnaHandle h, OnnaFrameCallback cb, void *user);

#ifdef __cplusplus
}
#endif
