#include "Snap7Loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

Snap7Loader &Snap7Loader::instance()
{
    static Snap7Loader s;
    return s;
}

bool Snap7Loader::load(QString *error)
{
    if (m_loaded)
        return true;

#ifdef _WIN32
    QStringList candidates;
    const QString env = qEnvironmentVariable("SNAP7_DIR");
    if (!env.isEmpty()) {
        candidates << env + QStringLiteral("/win64/snap7.dll");
        candidates << env + QStringLiteral("/snap7.dll");
        candidates << env + QStringLiteral("/bin/snap7.dll");
    }
    candidates << QCoreApplication::applicationDirPath() + QStringLiteral("/snap7.dll");
    candidates << QDir(QCoreApplication::applicationDirPath())
                      .absoluteFilePath(QStringLiteral("../../third_party/snap7/win64/snap7.dll"));
    candidates << QDir(QCoreApplication::applicationDirPath())
                      .absoluteFilePath(QStringLiteral("../../../third_party/snap7/win64/snap7.dll"));
    candidates << QStringLiteral("snap7.dll");

    HMODULE mod = nullptr;
    for (const QString &c : candidates) {
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(c));
        if (path != QLatin1String("snap7.dll") && !QFileInfo::exists(path))
            continue;
        mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()));
        if (mod) {
            m_path = path;
            break;
        }
    }
    if (!mod) {
        if (error)
            *error = QStringLiteral("snap7.dll not found (set SNAP7_DIR or copy to exe dir)");
        return false;
    }
    m_lib = mod;
    create = reinterpret_cast<Cli_Create_t>(GetProcAddress(mod, "Cli_Create"));
    destroy = reinterpret_cast<Cli_Destroy_t>(GetProcAddress(mod, "Cli_Destroy"));
    connectTo = reinterpret_cast<Cli_ConnectTo_t>(GetProcAddress(mod, "Cli_ConnectTo"));
    disconnect = reinterpret_cast<Cli_Disconnect_t>(GetProcAddress(mod, "Cli_Disconnect"));
    dbRead = reinterpret_cast<Cli_DBRead_t>(GetProcAddress(mod, "Cli_DBRead"));
    dbWrite = reinterpret_cast<Cli_DBWrite_t>(GetProcAddress(mod, "Cli_DBWrite"));
    if (!create || !destroy || !connectTo || !disconnect || !dbRead || !dbWrite) {
        if (error)
            *error = QStringLiteral("snap7.dll missing required exports");
        FreeLibrary(mod);
        m_lib = nullptr;
        return false;
    }
    m_loaded = true;
    return true;
#else
    if (error)
        *error = QStringLiteral("Snap7Loader only implemented on Windows");
    return false;
#endif
}
