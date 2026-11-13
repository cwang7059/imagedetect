#include "OnnaLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

OnnaLoader &OnnaLoader::instance()
{
    static OnnaLoader s;
    return s;
}

bool OnnaLoader::load(QString *error)
{
    if (m_loaded)
        return true;
#ifdef _WIN32
    const QStringList candidates = {
        qEnvironmentVariable("ONNA_SDK_DIR") + QStringLiteral("/OnnaCamera.dll"),
        qEnvironmentVariable("ONNA_SDK_DIR") + QStringLiteral("/bin/OnnaCamera.dll"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/OnnaCamera.dll"),
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../../third_party/onna/bin/OnnaCamera.dll")),
        QStringLiteral("OnnaCamera.dll"),
    };
    HMODULE mod = nullptr;
    for (const QString &c : candidates) {
        if (c.isEmpty())
            continue;
        const QString path = QDir::toNativeSeparators(c);
        if (path != QLatin1String("OnnaCamera.dll") && !QFileInfo::exists(path))
            continue;
        mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (mod)
            break;
    }
    if (!mod) {
        if (error)
            *error = QStringLiteral("OnnaCamera.dll not found (set ONNA_SDK_DIR)");
        return false;
    }
    m_lib = mod;
    init = reinterpret_cast<decltype(init)>(GetProcAddress(mod, "Onna_Init"));
    shutdown = reinterpret_cast<decltype(shutdown)>(GetProcAddress(mod, "Onna_Shutdown"));
    open = reinterpret_cast<decltype(open)>(GetProcAddress(mod, "Onna_Open"));
    close = reinterpret_cast<decltype(close)>(GetProcAddress(mod, "Onna_Close"));
    setTriggerMode = reinterpret_cast<decltype(setTriggerMode)>(GetProcAddress(mod, "Onna_SetTriggerMode"));
    start = reinterpret_cast<decltype(start)>(GetProcAddress(mod, "Onna_Start"));
    stop = reinterpret_cast<decltype(stop)>(GetProcAddress(mod, "Onna_Stop"));
    grabBgr = reinterpret_cast<decltype(grabBgr)>(GetProcAddress(mod, "Onna_GrabBgr"));
    if (!init || !open || !close || !start || !stop || !grabBgr) {
        if (error)
            *error = QStringLiteral("OnnaCamera.dll missing exports");
        FreeLibrary(mod);
        m_lib = nullptr;
        return false;
    }
    init();
    m_loaded = true;
    return true;
#else
    if (error)
        *error = QStringLiteral("OnnaLoader Windows-only");
    return false;
#endif
}
