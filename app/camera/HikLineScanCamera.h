#pragma once

#include "ICamera.h"
#include "SimLineScanCamera.h"

#include <QMutex>
#include <QByteArray>

/**
 * Hikvision MVS line-scan wrapper.
 * Compiled with USE_HIK_MVS when MVS Development kit is detected.
 * Falls back to SimLineScanCamera otherwise.
 */
class HikLineScanCamera : public ICamera
{
    Q_OBJECT
public:
    HikLineScanCamera(StationId station, const QString &id, QObject *parent = nullptr);
    ~HikLineScanCamera() override;

    QString cameraId() const override;
    StationId station() const override;

    bool openCamera(QString *error = nullptr) override;
    void closeCamera() override;
    bool isOpen() const override;

    bool onHardTrigger(quint16 slot, quint32 productSeq, QString *error = nullptr) override;

    bool usingRealSdk() const;

    void setDeviceIndex(unsigned int index);
    void setSerialNumber(const QString &serial);
    /** When false (default), skip MVS EnumDevices and use simulation. */
    void setUseReal(bool on) { m_useReal = on; }
    bool useReal() const { return m_useReal; }

private:
#ifdef USE_HIK_MVS
    bool openMvs(QString *error);
    void closeMvs();
    bool grabOneBgr(QByteArray *bgr, int *w, int *h, int timeoutMs, QString *error);
#endif

    StationId m_station;
    QString m_id;
    unsigned int m_deviceIndex = 0;
    QString m_serial;
    bool m_useReal = false;
    SimLineScanCamera *m_sim = nullptr;

#ifdef USE_HIK_MVS
    void *m_handle = nullptr;
    bool m_open = false;
    QByteArray m_frameBuf;
    QMutex m_pendingMutex;
    quint16 m_pendingSlot = 0;
    quint32 m_pendingSeq = 0;
#else
    bool m_open = false;
#endif
};
