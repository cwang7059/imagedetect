#pragma once

#include "snap7_c_api.h"

#include <QString>

/** Runtime loader for snap7.dll (no link-time dependency). */
class Snap7Loader
{
public:
    static Snap7Loader &instance();

    bool load(QString *error = nullptr);
    bool isLoaded() const { return m_loaded; }
    QString dllPath() const { return m_path; }

    Cli_Create_t create = nullptr;
    Cli_Destroy_t destroy = nullptr;
    Cli_ConnectTo_t connectTo = nullptr;
    Cli_Disconnect_t disconnect = nullptr;
    Cli_DBRead_t dbRead = nullptr;
    Cli_DBWrite_t dbWrite = nullptr;

private:
    Snap7Loader() = default;
    bool m_loaded = false;
    QString m_path;
    void *m_lib = nullptr; // HMODULE
};
