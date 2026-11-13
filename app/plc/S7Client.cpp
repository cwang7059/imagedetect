#include "S7Client.h"
#include "MockPlcClient.h"
#include "Snap7Loader.h"

class S7Client::Impl
{
public:
    MockPlcClient mock;
    S7Object client = 0;
    bool connected = false;
    bool useReal = false;
};

S7Client::S7Client(QObject *parent)
    : IPlcClient(parent)
    , m_impl(new Impl)
{
    connect(&m_impl->mock, &IPlcClient::connectedChanged, this, &IPlcClient::connectedChanged);
    connect(&m_impl->mock, &IPlcClient::communicationError, this, &IPlcClient::communicationError);
}

S7Client::~S7Client()
{
    disconnectFromPlc();
    delete m_impl;
}

bool S7Client::usingRealSnap7() const
{
    return m_impl->useReal;
}

bool S7Client::connectToPlc(const PlcConnectionConfig &cfg, QString *error)
{
    QString loadErr;
    if (Snap7Loader::instance().load(&loadErr)) {
        auto &s7 = Snap7Loader::instance();
        if (!m_impl->client)
            m_impl->client = s7.create();
        const int r = s7.connectTo(m_impl->client, cfg.ip.toUtf8().constData(), cfg.rack, cfg.slot);
        if (r != 0) {
            if (error)
                *error = QStringLiteral("Snap7 ConnectTo failed: %1 (%2)").arg(r).arg(cfg.ip);
            m_impl->connected = false;
            m_impl->useReal = false;
            // Fall back to mock so offline HMI still works
            return m_impl->mock.connectToPlc(cfg, error);
        }
        m_impl->connected = true;
        m_impl->useReal = true;
        emit connectedChanged(true);
        return true;
    }

    // No DLL → in-process mock
    m_impl->useReal = false;
    return m_impl->mock.connectToPlc(cfg, error);
}

void S7Client::disconnectFromPlc()
{
    if (m_impl->useReal && m_impl->client && Snap7Loader::instance().isLoaded()) {
        Snap7Loader::instance().disconnect(m_impl->client);
        Snap7Loader::instance().destroy(&m_impl->client);
        m_impl->client = 0;
        m_impl->connected = false;
        m_impl->useReal = false;
        emit connectedChanged(false);
        return;
    }
    m_impl->mock.disconnectFromPlc();
    m_impl->connected = false;
    m_impl->useReal = false;
}

bool S7Client::isConnected() const
{
    if (m_impl->useReal)
        return m_impl->connected;
    return m_impl->mock.isConnected();
}

bool S7Client::readDb(int db, int offset, int size, QByteArray *out, QString *error)
{
    if (!m_impl->useReal)
        return m_impl->mock.readDb(db, offset, size, out, error);

    if (!m_impl->connected || !m_impl->client) {
        if (error)
            *error = QStringLiteral("S7 not connected");
        return false;
    }
    QByteArray buf(size, 0);
    const int r = Snap7Loader::instance().dbRead(m_impl->client, db, offset, size, buf.data());
    if (r != 0) {
        if (error)
            *error = QStringLiteral("S7 DBRead failed: %1").arg(r);
        emit communicationError(error ? *error : QStringLiteral("DBRead fail"));
        return false;
    }
    if (out)
        *out = buf;
    return true;
}

bool S7Client::writeDb(int db, int offset, const QByteArray &data, QString *error)
{
    if (!m_impl->useReal)
        return m_impl->mock.writeDb(db, offset, data, error);

    if (!m_impl->connected || !m_impl->client) {
        if (error)
            *error = QStringLiteral("S7 not connected");
        return false;
    }
    const int r = Snap7Loader::instance().dbWrite(
        m_impl->client, db, offset, data.size(), const_cast<char *>(data.constData()));
    if (r != 0) {
        if (error)
            *error = QStringLiteral("S7 DBWrite failed: %1").arg(r);
        emit communicationError(error ? *error : QStringLiteral("DBWrite fail"));
        return false;
    }
    return true;
}
