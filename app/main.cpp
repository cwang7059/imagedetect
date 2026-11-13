#include "MainWindow.h"
#include "PythonBridge.h"
#include "camera/FramePacket.h"
#include "core/ProductRecord.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

static QString findProjectRoot()
{
    // Prefer executable-relative: app/../ or cwd
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath(QStringLiteral("../..")), // app/release -> repo
        QDir(appDir).absoluteFilePath(QStringLiteral("..")),
        QDir::current().absolutePath(),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../..")),
    };
    for (const QString &c : candidates) {
        const QDir d(c);
        if (d.exists(QStringLiteral("bridge/algo_api.py")) && d.exists(QStringLiteral("src/pipeline.py")))
            return QDir::cleanPath(d.absolutePath());
    }
    return QDir::cleanPath(QDir(appDir).absoluteFilePath(QStringLiteral("../..")));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qRegisterMetaType<TrackMeta>("TrackMeta");
    qRegisterMetaType<FramePacket>("FramePacket");
    qRegisterMetaType<ProductRecord>("ProductRecord");
    qRegisterMetaType<StationOdResult>("StationOdResult");
    qRegisterMetaType<StationLengthResult>("StationLengthResult");
    qRegisterMetaType<StationDefectResult>("StationDefectResult");

    const QString root = findProjectRoot();
    if (!QFileInfo::exists(root + QStringLiteral("/bridge/algo_api.py"))) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("路径错误"),
            QStringLiteral("找不到 bridge/algo_api.py。\n请从仓库根目录运行，或把可执行文件放在 app/release。\n当前探测根: %1")
                .arg(root));
        return 1;
    }

    MainWindow w(root);
    w.show();
    return app.exec();
}
