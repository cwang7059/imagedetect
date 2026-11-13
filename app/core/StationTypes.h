#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

enum class StationId : int {
    None = 0,
    ST1_Od = 1,
    ST2_Length = 2,
    ST3_Defect = 3,
    ST4_Judge = 4
};

enum class FailCode : quint16 {
    None = 0,
    OdOutOfTol = 1,
    LengthOutOfTol = 2,
    Defect = 3,
    TimeoutMissing = 4,
    AlgoError = 5,
    CommError = 6,
    Unknown = 7
};

inline QString stationName(StationId id)
{
    switch (id) {
    case StationId::ST1_Od: return QStringLiteral("ST1");
    case StationId::ST2_Length: return QStringLiteral("ST2");
    case StationId::ST3_Defect: return QStringLiteral("ST3");
    case StationId::ST4_Judge: return QStringLiteral("ST4");
    default: return QStringLiteral("NONE");
    }
}

inline QString failCodeName(FailCode c)
{
    switch (c) {
    case FailCode::None: return QStringLiteral("None");
    case FailCode::OdOutOfTol: return QStringLiteral("OdOutOfTol");
    case FailCode::LengthOutOfTol: return QStringLiteral("LengthOutOfTol");
    case FailCode::Defect: return QStringLiteral("Defect");
    case FailCode::TimeoutMissing: return QStringLiteral("TimeoutMissing");
    case FailCode::AlgoError: return QStringLiteral("AlgoError");
    case FailCode::CommError: return QStringLiteral("CommError");
    default: return QStringLiteral("Unknown");
    }
}
