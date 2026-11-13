#include "Recipe.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

Recipe Recipe::loadFromFile(const QString &path, QString *error)
{
    Recipe r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open recipe: %1").arg(path);
        return r;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid recipe JSON: %1").arg(path);
        return r;
    }
    const QJsonObject o = doc.object();
    r.id = o.value(QStringLiteral("id")).toInt(1);
    r.name = o.value(QStringLiteral("name")).toString(QStringLiteral("default"));
    r.dNomMm = o.value(QStringLiteral("d_nom_mm")).toDouble(8.0);
    r.dTolMm = o.value(QStringLiteral("d_tol_mm")).toDouble(0.15);
    r.lNomMm = o.value(QStringLiteral("l_nom_mm")).toDouble(60.0);
    r.lTolMm = o.value(QStringLiteral("l_tol_mm")).toDouble(0.30);
    r.maxDefectGrade = o.value(QStringLiteral("max_defect_grade")).toInt(1);
    r.resultTimeoutMs = o.value(QStringLiteral("result_timeout_ms")).toInt(800);
    r.saveNgImage = o.value(QStringLiteral("save_ng_image")).toBool(true);
    r.saveOkSampleRate = o.value(QStringLiteral("save_ok_sample_rate")).toDouble(0.02);
    return r;
}

bool Recipe::saveToFile(const QString &path, QString *error) const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("d_nom_mm"), dNomMm);
    o.insert(QStringLiteral("d_tol_mm"), dTolMm);
    o.insert(QStringLiteral("l_nom_mm"), lNomMm);
    o.insert(QStringLiteral("l_tol_mm"), lTolMm);
    o.insert(QStringLiteral("max_defect_grade"), maxDefectGrade);
    o.insert(QStringLiteral("result_timeout_ms"), resultTimeoutMs);
    o.insert(QStringLiteral("save_ng_image"), saveNgImage);
    o.insert(QStringLiteral("save_ok_sample_rate"), saveOkSampleRate);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Cannot write recipe: %1").arg(path);
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}
