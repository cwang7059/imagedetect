#pragma once

#include "StationTypes.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>

struct StationOdResult {
    bool ready = false;
    bool algoOk = false;
    double d1Mm = 0.0;
    double d2Mm = 0.0;
    double score = 0.0;
    qint64 elapsedMs = 0;
};

struct StationLengthResult {
    bool ready = false;
    bool algoOk = false;
    double lengthMm = 0.0;
    qint64 elapsedMs = 0;
};

struct StationDefectResult {
    bool ready = false;
    bool algoOk = false;
    int grade = 0;
    int defectCount = 0;
    QString summary;
    qint64 elapsedMs = 0;
};

struct ProductRecord {
    quint16 slot = 0;
    quint32 productSeq = 0;
    int recipeId = 1;
    QDateTime createdAt;
    QDateTime updatedAt;

    StationOdResult od;
    StationLengthResult length;
    StationDefectResult defect;

    bool judged = false;
    bool pass = false;
    FailCode failCode = FailCode::None;
    QString failReason;
    QString ngImagePath;

    bool allStationsReady() const
    {
        return od.ready && length.ready && defect.ready;
    }
};

Q_DECLARE_METATYPE(ProductRecord)
