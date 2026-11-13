#include "ProductTracker.h"

#include <QMutexLocker>

ProductTracker::ProductTracker(QObject *parent)
    : QObject(parent)
{
}

void ProductTracker::setJudge(FinalJudge *judge)
{
    QMutexLocker lock(&m_mutex);
    m_judge = judge;
}

void ProductTracker::setSlotCapacity(int n)
{
    QMutexLocker lock(&m_mutex);
    m_capacity = qMax(8, n);
}

void ProductTracker::clear()
{
    QMutexLocker lock(&m_mutex);
    m_slots.clear();
    m_okCount = 0;
    m_ngCount = 0;
}

ProductRecord &ProductTracker::ensureLocked(quint16 slot, quint32 productSeq)
{
    if (!m_slots.contains(slot)) {
        if (m_slots.size() >= m_capacity) {
            // Drop oldest by createdAt
            auto it = m_slots.begin();
            auto oldest = it;
            for (; it != m_slots.end(); ++it) {
                if (it.value().createdAt < oldest.value().createdAt)
                    oldest = it;
            }
            m_slots.erase(oldest);
        }
        ProductRecord rec;
        rec.slot = slot;
        rec.productSeq = productSeq;
        rec.createdAt = QDateTime::currentDateTime();
        rec.updatedAt = rec.createdAt;
        m_slots.insert(slot, rec);
    }
    ProductRecord &rec = m_slots[slot];
    if (productSeq != 0)
        rec.productSeq = productSeq;
    rec.updatedAt = QDateTime::currentDateTime();
    return rec;
}

ProductRecord ProductTracker::upsertSlot(quint16 slot, quint32 productSeq)
{
    QMutexLocker lock(&m_mutex);
    return ensureLocked(slot, productSeq);
}

void ProductTracker::setOd(quint16 slot, const StationOdResult &r, quint32 productSeq)
{
    ProductRecord copy;
    {
        QMutexLocker lock(&m_mutex);
        ProductRecord &rec = ensureLocked(slot, productSeq);
        rec.od = r;
        copy = rec;
    }
    emit recordUpdated(copy);
}

void ProductTracker::setLength(quint16 slot, const StationLengthResult &r, quint32 productSeq)
{
    ProductRecord copy;
    {
        QMutexLocker lock(&m_mutex);
        ProductRecord &rec = ensureLocked(slot, productSeq);
        rec.length = r;
        copy = rec;
    }
    emit recordUpdated(copy);
}

void ProductTracker::setDefect(quint16 slot, const StationDefectResult &r, quint32 productSeq)
{
    ProductRecord copy;
    {
        QMutexLocker lock(&m_mutex);
        ProductRecord &rec = ensureLocked(slot, productSeq);
        rec.defect = r;
        copy = rec;
    }
    emit recordUpdated(copy);
}

void ProductTracker::emitJudged(ProductRecord &rec)
{
    if (rec.pass)
        ++m_okCount;
    else
        ++m_ngCount;
    const ProductRecord copy = rec;
    // unlock before emit — caller holds lock; we emit after unlock in tryJudge
    Q_UNUSED(copy);
}

bool ProductTracker::tryJudge(quint16 slot, ProductRecord *out)
{
    ProductRecord copy;
    bool made = false;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_slots.contains(slot) || !m_judge)
            return false;
        ProductRecord &rec = m_slots[slot];
        if (rec.judged || !rec.allStationsReady())
            return false;
        m_judge->judge(&rec);
        if (rec.pass)
            ++m_okCount;
        else
            ++m_ngCount;
        copy = rec;
        made = true;
    }
    if (made) {
        emit judged(copy);
        if (out)
            *out = copy;
    }
    return made;
}

bool ProductTracker::forceTimeoutJudge(quint16 slot, ProductRecord *out)
{
    ProductRecord copy;
    bool made = false;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_slots.contains(slot))
            return false;
        ProductRecord &rec = m_slots[slot];
        if (rec.judged)
            return false;
        rec.judged = true;
        rec.pass = false;
        rec.failCode = FailCode::TimeoutMissing;
        rec.failReason = QStringLiteral("Timeout or missing station");
        ++m_ngCount;
        copy = rec;
        made = true;
    }
    if (made) {
        emit judged(copy);
        if (out)
            *out = copy;
    }
    return made;
}

bool ProductTracker::get(quint16 slot, ProductRecord *out) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_slots.contains(slot) || !out)
        return false;
    *out = m_slots.value(slot);
    return true;
}

QList<ProductRecord> ProductTracker::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    return m_slots.values();
}
