#pragma once

#include <QString>

struct Recipe {
    int id = 1;
    QString name = QStringLiteral("default");
    double dNomMm = 8.0;
    double dTolMm = 0.15;
    double lNomMm = 60.0;
    double lTolMm = 0.30;
    int maxDefectGrade = 1;
    int resultTimeoutMs = 800;
    bool saveNgImage = true;
    double saveOkSampleRate = 0.02;

    static Recipe loadFromFile(const QString &path, QString *error = nullptr);
    bool saveToFile(const QString &path, QString *error = nullptr) const;
};
