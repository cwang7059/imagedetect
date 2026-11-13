#pragma once

#include "onna_camera_c_api.h"

#include <QString>

class OnnaLoader
{
public:
    static OnnaLoader &instance();
    bool load(QString *error = nullptr);
    bool isLoaded() const { return m_loaded; }

    decltype(&Onna_Init) init = nullptr;
    decltype(&Onna_Shutdown) shutdown = nullptr;
    decltype(&Onna_Open) open = nullptr;
    decltype(&Onna_Close) close = nullptr;
    decltype(&Onna_SetTriggerMode) setTriggerMode = nullptr;
    decltype(&Onna_Start) start = nullptr;
    decltype(&Onna_Stop) stop = nullptr;
    decltype(&Onna_GrabBgr) grabBgr = nullptr;

private:
    OnnaLoader() = default;
    bool m_loaded = false;
    void *m_lib = nullptr;
};
