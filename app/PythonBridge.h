#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QMutex>
#include <QString>

struct TrackMeta {
    bool locked = false;
    float x = 0.f;
    float y = 0.f;
    float missX = 0.f;
    float missY = 0.f;
    int lost = 0;
    int threshold = 180;
    int gate = 80;
    float detX = -1.f;
    float detY = -1.f;
    QString backend;
    QString mode; // bright | face
    QByteArray mask; // uint8 HxW
    int maskH = 0;
    int maskW = 0;
};

struct OdMeasureResult {
    bool ok = false;
    double d1Mm = 0.0;
    double d2Mm = 0.0;
    double score = 0.0;
};

struct LengthMeasureResult {
    bool ok = false;
    double lengthMm = 0.0;
};

struct DefectMeasureResult {
    bool ok = false;
    int grade = 0;
    int defectCount = 0;
    QString summary;
};

/**
 * Embeds CPython and calls bridge.algo_api.
 * Not thread-safe across instances; one bridge, call from one worker thread.
 */
class PythonBridge
{
public:
    PythonBridge();
    ~PythonBridge();

    PythonBridge(const PythonBridge &) = delete;
    PythonBridge &operator=(const PythonBridge &) = delete;

    bool initialize(const QString &projectRoot, QString *error = nullptr);
    void finalize();
    bool isReady() const { return m_ready; }

    bool initPipeline(int threshold = 180, int gateSize = 80, QString *error = nullptr);
    bool reset(QString *error = nullptr);
    bool setThreshold(int v, QString *error = nullptr);
    bool setGate(int v, QString *error = nullptr);
    bool setMode(const QString &mode, QString *error = nullptr); // "bright" | "face"

    /** bgr: contiguous BGR8 buffer, size h*w*3 */
    bool process(const uchar *bgr, int h, int w, TrackMeta *out, QString *error = nullptr);

    bool loadCalibDir(const QString &calibDir, QString *error = nullptr);
    bool measureOd(const uchar *bgr, int h, int w, OdMeasureResult *out, QString *error = nullptr);
    bool measureLength(const uchar *bgr, int h, int w, LengthMeasureResult *out, QString *error = nullptr);
    bool inspectDefect(const uchar *bgr, int h, int w, DefectMeasureResult *out, QString *error = nullptr);

    /** Optional I/O helpers (cv2) when C++ OpenCV is unavailable */
    bool grabOpen(const QString &source, QString *error = nullptr); // "0" / path
    bool grabRead(QByteArray *bgr, int *h, int *w, QString *error = nullptr);
    void grabClose();

private:
    bool callVoid(const char *fn, QString *error);
    bool callSetInt(const char *fn, int v, QString *error);
    QString pyErrToString();

    bool m_ready = false;
    bool m_ownPython = false;
    void *m_module = nullptr; // PyObject*
};

Q_DECLARE_METATYPE(TrackMeta)
