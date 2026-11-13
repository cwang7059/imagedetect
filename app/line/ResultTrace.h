#pragma once

#include "core/ProductRecord.h"

#include <QObject>
#include <QString>

/** Append-only CSV trace + optional NG image directory. */
class ResultTrace : public QObject
{
    Q_OBJECT
public:
    explicit ResultTrace(QObject *parent = nullptr);

    bool openLog(const QString &csvPath, QString *error = nullptr);
    void setImageDir(const QString &dir);
    void closeLog();

    void append(const ProductRecord &rec);
    QString saveNgImage(const ProductRecord &rec, const QByteArray &bgr, int w, int h, int channels = 3);

signals:
    void logged(const ProductRecord &rec);

private:
    QString m_csvPath;
    QString m_imageDir;
    bool m_headerWritten = false;
};
