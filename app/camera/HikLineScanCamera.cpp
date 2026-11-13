#include "HikLineScanCamera.h"

#include <cstring>

#include <QDateTime>

#ifdef USE_HIK_MVS
#  include "MvCameraControl.h"
#endif

HikLineScanCamera::HikLineScanCamera(StationId station, const QString &id, QObject *parent)
    : ICamera(parent)
    , m_station(station)
    , m_id(id)
{
    // Always keep a sim fallback for dry-run when no physical camera is present.
    m_sim = new SimLineScanCamera(station, id + QStringLiteral("_sim"), this);
    connect(m_sim, &ICamera::frameReady, this, &ICamera::frameReady);
    connect(m_sim, &ICamera::cameraError, this, &ICamera::cameraError);
    connect(m_sim, &ICamera::onlineChanged, this, &ICamera::onlineChanged);
}

HikLineScanCamera::~HikLineScanCamera()
{
    closeCamera();
}

void HikLineScanCamera::setDeviceIndex(unsigned int index)
{
    m_deviceIndex = index;
}

void HikLineScanCamera::setSerialNumber(const QString &serial)
{
    m_serial = serial;
}

QString HikLineScanCamera::cameraId() const
{
    return m_id;
}

StationId HikLineScanCamera::station() const
{
    return m_station;
}

bool HikLineScanCamera::usingRealSdk() const
{
#ifdef USE_HIK_MVS
    return m_open && m_handle != nullptr;
#else
    return false;
#endif
}

bool HikLineScanCamera::openCamera(QString *error)
{
#ifdef USE_HIK_MVS
    // GigE EnumDevices can block for a long time on networks with no camera.
    // Only probe MVS when explicitly enabled (cameras.json use_real / env).
    if (m_useReal) {
        QString mvsErr;
        if (openMvs(&mvsErr))
            return true;
        if (error)
            *error = mvsErr + QStringLiteral(" (fallback sim)");
        return m_sim->openCamera(error);
    }
#endif
    return m_sim->openCamera(error);
}

void HikLineScanCamera::closeCamera()
{
#ifdef USE_HIK_MVS
    closeMvs();
#endif
    if (m_sim)
        m_sim->closeCamera();
}

bool HikLineScanCamera::isOpen() const
{
#ifdef USE_HIK_MVS
    if (m_open && m_handle)
        return true;
#endif
    return m_sim && m_sim->isOpen();
}

bool HikLineScanCamera::onHardTrigger(quint16 slot, quint32 productSeq, QString *error)
{
#ifdef USE_HIK_MVS
    if (!m_open || !m_handle) {
        return m_sim->onHardTrigger(slot, productSeq, error);
    }
    {
        QMutexLocker lock(&m_pendingMutex);
        m_pendingSlot = slot;
        m_pendingSeq = productSeq;
    }
    QByteArray bgr;
    int w = 0, h = 0;
    // Hardware trigger should already have filled SDK buffer; pull with short wait.
    if (!grabOneBgr(&bgr, &w, &h, 800, error))
        return false;

    FramePacket pkt;
    pkt.station = m_station;
    pkt.slot = slot;
    pkt.productSeq = productSeq;
    pkt.timestampMs = QDateTime::currentMSecsSinceEpoch();
    pkt.width = w;
    pkt.height = h;
    pkt.channels = 3;
    pkt.buffer = bgr;
    pkt.cameraId = m_id;
    emit frameReady(pkt);
    return true;
#else
    return m_sim->onHardTrigger(slot, productSeq, error);
#endif
}

#ifdef USE_HIK_MVS

bool HikLineScanCamera::openMvs(QString *error)
{
    if (m_open)
        return true;

    MV_CC_DEVICE_INFO_LIST deviceList;
    memset(&deviceList, 0, sizeof(deviceList));
    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE | MV_CAMERALINK_DEVICE, &deviceList);
    if (nRet != MV_OK) {
        if (error)
            *error = QStringLiteral("MV_CC_EnumDevices failed: 0x%1").arg(nRet, 0, 16);
        return false;
    }
    if (deviceList.nDeviceNum == 0) {
        if (error)
            *error = QStringLiteral("No Hikvision devices found");
        return false;
    }

    unsigned int index = m_deviceIndex;
    if (!m_serial.isEmpty()) {
        bool found = false;
        for (unsigned int i = 0; i < deviceList.nDeviceNum; ++i) {
            MV_CC_DEVICE_INFO *info = deviceList.pDeviceInfo[i];
            if (!info)
                continue;
            QString sn;
            if (info->nTLayerType == MV_GIGE_DEVICE) {
                sn = QString::fromLatin1(
                    reinterpret_cast<const char *>(info->SpecialInfo.stGigEInfo.chSerialNumber));
            } else if (info->nTLayerType == MV_USB_DEVICE) {
                sn = QString::fromLatin1(
                    reinterpret_cast<const char *>(info->SpecialInfo.stUsb3VInfo.chSerialNumber));
            }
            if (sn == m_serial) {
                index = i;
                found = true;
                break;
            }
        }
        if (!found) {
            if (error)
                *error = QStringLiteral("Hik serial not found: %1").arg(m_serial);
            return false;
        }
    }
    if (index >= deviceList.nDeviceNum) {
        if (error)
            *error = QStringLiteral("Hik device index out of range");
        return false;
    }

    nRet = MV_CC_CreateHandle(&m_handle, deviceList.pDeviceInfo[index]);
    if (nRet != MV_OK || !m_handle) {
        if (error)
            *error = QStringLiteral("MV_CC_CreateHandle failed: 0x%1").arg(nRet, 0, 16);
        m_handle = nullptr;
        return false;
    }
    nRet = MV_CC_OpenDevice(m_handle);
    if (nRet != MV_OK) {
        if (error)
            *error = QStringLiteral("MV_CC_OpenDevice failed: 0x%1").arg(nRet, 0, 16);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    // Line-scan production: hardware trigger on Line0 (PLC). Soft path still uses GetImage.
    MV_CC_SetEnumValue(m_handle, "TriggerMode", MV_TRIGGER_MODE_ON);
    MV_CC_SetEnumValue(m_handle, "TriggerSource", MV_TRIGGER_SOURCE_LINE0);

    nRet = MV_CC_StartGrabbing(m_handle);
    if (nRet != MV_OK) {
        if (error)
            *error = QStringLiteral("MV_CC_StartGrabbing failed: 0x%1").arg(nRet, 0, 16);
        MV_CC_CloseDevice(m_handle);
        MV_CC_DestroyHandle(m_handle);
        m_handle = nullptr;
        return false;
    }

    m_open = true;
    emit onlineChanged(true);
    return true;
}

void HikLineScanCamera::closeMvs()
{
    if (!m_handle)
        return;
    MV_CC_StopGrabbing(m_handle);
    MV_CC_CloseDevice(m_handle);
    MV_CC_DestroyHandle(m_handle);
    m_handle = nullptr;
    m_open = false;
    emit onlineChanged(false);
}

bool HikLineScanCamera::grabOneBgr(QByteArray *bgr, int *w, int *h, int timeoutMs, QString *error)
{
    if (!m_handle)
        return false;

    MVCC_INTVALUE stParam;
    memset(&stParam, 0, sizeof(stParam));
    int nRet = MV_CC_GetIntValue(m_handle, "PayloadSize", &stParam);
    unsigned int bufSize = (nRet == MV_OK && stParam.nCurValue > 0)
        ? stParam.nCurValue
        : 16u * 1024u * 1024u;
    // BGR may need width*height*3; allocate generously
    if (bufSize < 4u * 1024u * 1024u)
        bufSize = 16u * 1024u * 1024u;

    m_frameBuf.resize(static_cast<int>(bufSize));
    MV_FRAME_OUT_INFO_EX frameInfo;
    memset(&frameInfo, 0, sizeof(frameInfo));
    nRet = MV_CC_GetImageForBGR(
        m_handle,
        reinterpret_cast<unsigned char *>(m_frameBuf.data()),
        bufSize,
        &frameInfo,
        timeoutMs);
    if (nRet != MV_OK) {
        if (error)
            *error = QStringLiteral("MV_CC_GetImageForBGR failed: 0x%1").arg(nRet, 0, 16);
        return false;
    }
    const int width = static_cast<int>(frameInfo.nWidth);
    const int height = static_cast<int>(frameInfo.nHeight);
    const int bytes = width * height * 3;
    if (width <= 0 || height <= 0 || bytes <= 0 || bytes > m_frameBuf.size()) {
        if (error)
            *error = QStringLiteral("Invalid Hik frame size %1x%2").arg(width).arg(height);
        return false;
    }
    if (bgr)
        *bgr = m_frameBuf.left(bytes);
    if (w)
        *w = width;
    if (h)
        *h = height;
    return true;
}

#endif // USE_HIK_MVS
