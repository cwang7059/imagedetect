#pragma once

#include "ICamera.h"
#include "SimLineScanCamera.h"

/**
 * Onna (昂纳) line-scan for ST3 defect.
 * Loads OnnaCamera.dll via OnnaLoader when present; else simulation.
 */
class OnnaLineScanCamera : public ICamera
{
    Q_OBJECT
public:
    explicit OnnaLineScanCamera(const QString &id = QStringLiteral("onna_st3"),
                                QObject *parent = nullptr);
    ~OnnaLineScanCamera() override;

    QString cameraId() const override;
    StationId station() const override { return StationId::ST3_Defect; }

    bool openCamera(QString *error = nullptr) override;
    void closeCamera() override;
    bool isOpen() const override;

    bool onHardTrigger(quint16 slot, quint32 productSeq, QString *error = nullptr) override;

    bool usingRealSdk() const;

    void setDeviceIndex(int index);
    void setSerialNumber(const QString &serial);
    void setUseReal(bool on) { m_useRealWanted = on; }
    bool useRealWanted() const { return m_useRealWanted; }

private:
    QString m_id;
    int m_index = 0;
    QString m_serial;
    bool m_useRealWanted = false;
    SimLineScanCamera *m_sim = nullptr;
    void *m_handle = nullptr; // OnnaHandle
    bool m_open = false;
    bool m_real = false;
    QByteArray m_buf;
};
