#pragma once

#include "ICamera.h"

/** Synthetic line-scan frames for dry-run without vendor SDK. */
class SimLineScanCamera : public ICamera
{
    Q_OBJECT
public:
    SimLineScanCamera(StationId station, const QString &id, QObject *parent = nullptr);

    QString cameraId() const override { return m_id; }
    StationId station() const override { return m_station; }

    bool openCamera(QString *error = nullptr) override;
    void closeCamera() override;
    bool isOpen() const override { return m_open; }

    bool onHardTrigger(quint16 slot, quint32 productSeq, QString *error = nullptr) override;

    void setImageSize(int w, int h);

private:
    QByteArray makeFrame(quint16 slot) const;

    StationId m_station;
    QString m_id;
    bool m_open = false;
    int m_w = 640;
    int m_h = 240;
};
