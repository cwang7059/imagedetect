#pragma once

#include "FinalJudge.h"
#include "ProductRecord.h"

#include <QHash>
#include <QObject>
#include <QMutex>

/**
 * Slot-based product tracking across stations.
 * PLC advances slot indices; PC accumulates ST1/ST2/ST3 then judges.
 */
class ProductTracker : public QObject
{
    Q_OBJECT
public:
    explicit ProductTracker(QObject *parent = nullptr);

    void setJudge(FinalJudge *judge);
    void setSlotCapacity(int n);
    void clear();

    ProductRecord upsertSlot(quint16 slot, quint32 productSeq = 0);
    void setOd(quint16 slot, const StationOdResult &r, quint32 productSeq = 0);
    void setLength(quint16 slot, const StationLengthResult &r, quint32 productSeq = 0);
    void setDefect(quint16 slot, const StationDefectResult &r, quint32 productSeq = 0);

    /** Judge if all ready and not yet judged; returns true if a new judgment was made. */
    bool tryJudge(quint16 slot, ProductRecord *out = nullptr);

    /** Force judge missing stations as TimeoutMissing. */
    bool forceTimeoutJudge(quint16 slot, ProductRecord *out = nullptr);

    bool get(quint16 slot, ProductRecord *out) const;
    QList<ProductRecord> snapshot() const;

    int okCount() const { return m_okCount; }
    int ngCount() const { return m_ngCount; }

signals:
    void recordUpdated(const ProductRecord &rec);
    void judged(const ProductRecord &rec);

private:
    ProductRecord &ensureLocked(quint16 slot, quint32 productSeq);
    void emitJudged(ProductRecord &rec);

    mutable QMutex m_mutex;
    QHash<quint16, ProductRecord> m_slots;
    FinalJudge *m_judge = nullptr;
    int m_capacity = 256;
    int m_okCount = 0;
    int m_ngCount = 0;
};
