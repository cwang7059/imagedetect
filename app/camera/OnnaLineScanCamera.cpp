#include "OnnaLineScanCamera.h"
#include "OnnaLoader.h"

#include <cstring>

#include <QDateTime>

OnnaLineScanCamera::OnnaLineScanCamera(const QString &id, QObject *parent)
    : ICamera(parent)
    , m_id(id)
{
    m_sim = new SimLineScanCamera(StationId::ST3_Defect, id + QStringLiteral("_sim"), this);
    connect(m_sim, &ICamera::frameReady, this, &ICamera::frameReady);
    connect(m_sim, &ICamera::cameraError, this, &ICamera::cameraError);
    connect(m_sim, &ICamera::onlineChanged, this, &ICamera::onlineChanged);
}

OnnaLineScanCamera::~OnnaLineScanCamera()
{
    closeCamera();
}

void OnnaLineScanCamera::setDeviceIndex(int index)
{
    m_index = index;
}

void OnnaLineScanCamera::setSerialNumber(const QString &serial)
{
    m_serial = serial;
}

QString OnnaLineScanCamera::cameraId() const
{
    return m_id;
}

bool OnnaLineScanCamera::usingRealSdk() const
{
    return m_real;
}

bool OnnaLineScanCamera::openCamera(QString *error)
{
    if (!m_useRealWanted) {
        m_real = false;
        return m_sim->openCamera(error);
    }

    QString loadErr;
    if (OnnaLoader::instance().load(&loadErr)) {
        auto &api = OnnaLoader::instance();
        OnnaHandle h = nullptr;
        const QByteArray sn = m_serial.toUtf8();
        const int rc = api.open(&h, m_index, m_serial.isEmpty() ? nullptr : sn.constData());
        if (rc != 0 || !h) {
            if (error)
                *error = QStringLiteral("Onna_Open failed: %1").arg(rc);
            return m_sim->openCamera(error);
        }
        if (api.setTriggerMode)
            api.setTriggerMode(h, 1); // hardware
        if (api.start(h) != 0) {
            api.close(h);
            if (error)
                *error = QStringLiteral("Onna_Start failed");
            return m_sim->openCamera(error);
        }
        m_handle = h;
        m_open = true;
        m_real = true;
        emit onlineChanged(true);
        return true;
    }
    m_real = false;
    return m_sim->openCamera(error);
}

void OnnaLineScanCamera::closeCamera()
{
    if (m_real && m_handle && OnnaLoader::instance().isLoaded()) {
        auto &api = OnnaLoader::instance();
        api.stop(m_handle);
        api.close(m_handle);
        m_handle = nullptr;
        m_open = false;
        m_real = false;
        emit onlineChanged(false);
        return;
    }
    m_sim->closeCamera();
    m_open = false;
}

bool OnnaLineScanCamera::isOpen() const
{
    if (m_real)
        return m_open;
    return m_sim->isOpen();
}

bool OnnaLineScanCamera::onHardTrigger(quint16 slot, quint32 productSeq, QString *error)
{
    if (!m_real || !m_handle) {
        return m_sim->onHardTrigger(slot, productSeq, error);
    }
    auto &api = OnnaLoader::instance();
    m_buf.resize(32 * 1024 * 1024);
    OnnaFrameInfo info;
    memset(&info, 0, sizeof(info));
    const int rc = api.grabBgr(
        m_handle,
        reinterpret_cast<unsigned char *>(m_buf.data()),
        m_buf.size(),
        &info,
        800);
    if (rc != 0 || info.width <= 0 || info.height <= 0) {
        if (error)
            *error = QStringLiteral("Onna_GrabBgr failed: %1").arg(rc);
        return false;
    }
    const int ch = info.channels > 0 ? info.channels : 3;
    const int bytes = info.width * info.height * ch;
    FramePacket pkt;
    pkt.station = StationId::ST3_Defect;
    pkt.slot = slot;
    pkt.productSeq = productSeq;
    pkt.timestampMs = QDateTime::currentMSecsSinceEpoch();
    pkt.width = info.width;
    pkt.height = info.height;
    pkt.channels = ch;
    pkt.buffer = m_buf.left(bytes);
    pkt.cameraId = m_id;
    emit frameReady(pkt);
    return true;
}
