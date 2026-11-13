#include "ResultTrace.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

ResultTrace::ResultTrace(QObject *parent)
    : QObject(parent)
{
}

bool ResultTrace::openLog(const QString &csvPath, QString *error)
{
    m_csvPath = csvPath;
    QFileInfo fi(csvPath);
    QDir().mkpath(fi.absolutePath());
    QFile f(csvPath);
    const bool exists = f.exists() && f.size() > 0;
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot open trace CSV: %1").arg(csvPath);
        return false;
    }
    if (!exists) {
        QTextStream ts(&f);
        ts << "time,slot,seq,recipe_id,pass,fail_code,fail_reason,"
              "d1_mm,d2_mm,od_ok,od_ms,"
              "length_mm,length_ok,length_ms,"
              "defect_grade,defect_count,defect_ok,defect_ms,defect_summary,ng_image\n";
        m_headerWritten = true;
    } else {
        m_headerWritten = true;
    }
    return true;
}

void ResultTrace::setImageDir(const QString &dir)
{
    m_imageDir = dir;
    if (!dir.isEmpty())
        QDir().mkpath(dir);
}

void ResultTrace::closeLog()
{
    m_csvPath.clear();
}

static QString csvEscape(const QString &s)
{
    QString t = s;
    t.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(t);
}

void ResultTrace::append(const ProductRecord &rec)
{
    if (m_csvPath.isEmpty())
        return;
    QFile f(m_csvPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream ts(&f);
    const QString ngName = rec.ngImagePath.isEmpty()
        ? QString()
        : QFileInfo(rec.ngImagePath).fileName();
    ts << rec.updatedAt.toString(Qt::ISODate) << ','
       << rec.slot << ','
       << rec.productSeq << ','
       << rec.recipeId << ','
       << (rec.pass ? 1 : 0) << ','
       << static_cast<int>(rec.failCode) << ','
       << csvEscape(rec.failReason) << ','
       << rec.od.d1Mm << ','
       << rec.od.d2Mm << ','
       << (rec.od.algoOk ? 1 : 0) << ','
       << rec.od.elapsedMs << ','
       << rec.length.lengthMm << ','
       << (rec.length.algoOk ? 1 : 0) << ','
       << rec.length.elapsedMs << ','
       << rec.defect.grade << ','
       << rec.defect.defectCount << ','
       << (rec.defect.algoOk ? 1 : 0) << ','
       << rec.defect.elapsedMs << ','
       << csvEscape(rec.defect.summary) << ','
       << csvEscape(ngName) << '\n';
    emit logged(rec);
}

QString ResultTrace::saveNgImage(const ProductRecord &rec, const QByteArray &bgr, int w, int h, int channels)
{
    if (m_imageDir.isEmpty() || bgr.isEmpty() || w <= 0 || h <= 0)
        return QString();
    const QString name = QStringLiteral("NG_slot%1_seq%2_%3.raw")
                             .arg(rec.slot)
                             .arg(rec.productSeq)
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmsszzz")));
    const QString path = QDir(m_imageDir).filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    QByteArray hdr = QStringLiteral("%1 %2 %3\n").arg(w).arg(h).arg(channels).toUtf8();
    f.write(hdr);
    f.write(bgr);
    return path;
}
