#include "PythonBridge.h"

#pragma push_macro("slots")
#undef slots
#include <Python.h>
#pragma pop_macro("slots")

#include <QDir>
#include <QMutex>
#include <QMutexLocker>

namespace {

QRecursiveMutex g_pyMutex;
bool g_gilReleasedAfterInit = false;

/** Serialize C++ entry and acquire the Python GIL (required for multi-thread embed). */
struct PyLock {
    QMutexLocker locker;
    PyGILState_STATE gil {};
    bool gilHeld = false;

    PyLock()
        : locker(&g_pyMutex)
    {
        if (Py_IsInitialized()) {
            gil = PyGILState_Ensure();
            gilHeld = true;
        }
    }
    ~PyLock()
    {
        if (gilHeld)
            PyGILState_Release(gil);
    }
};

QString objectToQString(PyObject *obj)
{
    if (!obj)
        return QString();
    if (PyUnicode_Check(obj)) {
        const char *u = PyUnicode_AsUTF8(obj);
        return u ? QString::fromUtf8(u) : QString();
    }
    PyObject *s = PyObject_Str(obj);
    if (!s)
        return QString();
    const char *u = PyUnicode_AsUTF8(s);
    QString out = u ? QString::fromUtf8(u) : QString();
    Py_DECREF(s);
    return out;
}

double dictGetFloat(PyObject *d, const char *key, double def = 0.0)
{
    PyObject *v = PyDict_GetItemString(d, key); // borrowed
    if (!v)
        return def;
    if (PyFloat_Check(v))
        return PyFloat_AsDouble(v);
    if (PyLong_Check(v))
        return static_cast<double>(PyLong_AsLong(v));
    return def;
}

long dictGetLong(PyObject *d, const char *key, long def = 0)
{
    PyObject *v = PyDict_GetItemString(d, key);
    if (!v)
        return def;
    if (PyLong_Check(v))
        return PyLong_AsLong(v);
    if (PyFloat_Check(v))
        return static_cast<long>(PyFloat_AsDouble(v));
    return def;
}

bool dictGetBool(PyObject *d, const char *key, bool def = false)
{
    PyObject *v = PyDict_GetItemString(d, key);
    if (!v)
        return def;
    return PyObject_IsTrue(v) == 1;
}

} // namespace

PythonBridge::PythonBridge() = default;

PythonBridge::~PythonBridge()
{
    finalize();
}

QString PythonBridge::pyErrToString()
{
    if (!PyErr_Occurred())
        return QString();
    PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    QString msg = objectToQString(value);
    if (msg.isEmpty())
        msg = QStringLiteral("Python error");
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    return msg;
}

bool PythonBridge::initialize(const QString &projectRoot, QString *error)
{
    QMutexLocker gate(&g_pyMutex);
    if (m_ready)
        return true;

    if (!Py_IsInitialized()) {
        Py_Initialize();
        m_ownPython = true;
        if (!Py_IsInitialized()) {
            if (error)
                *error = QStringLiteral("Py_Initialize failed");
            return false;
        }
        // Drop the implicit GIL so other threads can PyGILState_Ensure().
        PyEval_SaveThread();
        g_gilReleasedAfterInit = true;
    }

    const PyGILState_STATE gil = PyGILState_Ensure();

    PyObject *sysPath = PySys_GetObject("path"); // borrowed
    if (!sysPath) {
        PyGILState_Release(gil);
        if (error)
            *error = QStringLiteral("sys.path unavailable");
        return false;
    }

    QByteArray rootUtf8 = QDir::toNativeSeparators(projectRoot).toUtf8();
    PyObject *pRoot = PyUnicode_FromString(rootUtf8.constData());
    if (!pRoot) {
        const QString err = pyErrToString();
        PyGILState_Release(gil);
        if (error)
            *error = err;
        return false;
    }
    const int found = PySequence_Contains(sysPath, pRoot);
    if (found == 0)
        PyList_Insert(sysPath, 0, pRoot);
    Py_DECREF(pRoot);

    PyObject *mod = PyImport_ImportModule("bridge.algo_api");
    if (!mod) {
        const QString err = QStringLiteral("import bridge.algo_api failed: %1").arg(pyErrToString());
        PyGILState_Release(gil);
        if (error)
            *error = err;
        return false;
    }
    m_module = mod;
    m_ready = true;
    PyGILState_Release(gil);
    return true;
}

void PythonBridge::finalize()
{
    PyLock lock;
    grabClose();
    if (m_module) {
        Py_DECREF(static_cast<PyObject *>(m_module));
        m_module = nullptr;
    }
    m_ready = false;
    // Never Py_Finalize() while other worker bridges may still be alive.
    m_ownPython = false;
}

bool PythonBridge::initPipeline(int threshold, int gateSize, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module) {
        if (error)
            *error = QStringLiteral("PythonBridge not ready");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "init");
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallFunction(fn, "ii", threshold, gateSize);
    Py_DECREF(fn);
    if (!ret) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::callVoid(const char *fnName, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module) {
        if (error)
            *error = QStringLiteral("PythonBridge not ready");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), fnName);
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallObject(fn, nullptr);
    Py_DECREF(fn);
    if (!ret) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::callSetInt(const char *fnName, int v, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module) {
        if (error)
            *error = QStringLiteral("PythonBridge not ready");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), fnName);
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallFunction(fn, "i", v);
    Py_DECREF(fn);
    if (!ret) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::reset(QString *error)
{
    return callVoid("reset", error);
}

bool PythonBridge::setThreshold(int v, QString *error)
{
    return callSetInt("set_threshold", v, error);
}

bool PythonBridge::setGate(int v, QString *error)
{
    return callSetInt("set_gate", v, error);
}

bool PythonBridge::setMode(const QString &mode, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module) {
        if (error)
            *error = QStringLiteral("PythonBridge not ready");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "set_mode");
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    QByteArray m = mode.toUtf8();
    PyObject *ret = PyObject_CallFunction(fn, "s", m.constData());
    Py_DECREF(fn);
    if (!ret) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::process(const uchar *bgr, int h, int w, TrackMeta *out, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module || !bgr || !out || h <= 0 || w <= 0) {
        if (error)
            *error = QStringLiteral("invalid process args");
        return false;
    }

    const Py_ssize_t nbytes = static_cast<Py_ssize_t>(h) * w * 3;
    PyObject *bytesObj = PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bgr), nbytes);
    if (!bytesObj) {
        if (error)
            *error = pyErrToString();
        return false;
    }

    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "process");
    if (!fn) {
        Py_DECREF(bytesObj);
        if (error)
            *error = pyErrToString();
        return false;
    }

    PyObject *ret = PyObject_CallFunction(fn, "Oiii", bytesObj, h, w, 3);
    Py_DECREF(fn);
    Py_DECREF(bytesObj);
    if (!ret || !PyDict_Check(ret)) {
        if (error)
            *error = ret ? QStringLiteral("process did not return dict") : pyErrToString();
        Py_XDECREF(ret);
        return false;
    }

    out->locked = dictGetBool(ret, "locked");
    out->x = static_cast<float>(dictGetFloat(ret, "x"));
    out->y = static_cast<float>(dictGetFloat(ret, "y"));
    out->missX = static_cast<float>(dictGetFloat(ret, "miss_x"));
    out->missY = static_cast<float>(dictGetFloat(ret, "miss_y"));
    out->lost = static_cast<int>(dictGetLong(ret, "lost"));
    out->threshold = static_cast<int>(dictGetLong(ret, "threshold", 180));
    out->gate = static_cast<int>(dictGetLong(ret, "gate", 80));
    out->detX = static_cast<float>(dictGetFloat(ret, "det_x", -1.0));
    out->detY = static_cast<float>(dictGetFloat(ret, "det_y", -1.0));
    out->maskH = static_cast<int>(dictGetLong(ret, "mask_h"));
    out->maskW = static_cast<int>(dictGetLong(ret, "mask_w"));

    PyObject *backend = PyDict_GetItemString(ret, "backend");
    out->backend = objectToQString(backend);
    PyObject *modeObj = PyDict_GetItemString(ret, "mode");
    out->mode = objectToQString(modeObj);

    PyObject *maskObj = PyDict_GetItemString(ret, "mask");
    out->mask.clear();
    if (maskObj && PyBytes_Check(maskObj)) {
        char *buf = nullptr;
        Py_ssize_t n = 0;
        if (PyBytes_AsStringAndSize(maskObj, &buf, &n) == 0 && buf && n > 0)
            out->mask = QByteArray(buf, static_cast<int>(n));
    }

    Py_DECREF(ret);
    return true;
}

bool PythonBridge::loadCalibDir(const QString &calibDir, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module) {
        if (error)
            *error = QStringLiteral("PythonBridge not ready");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "load_calib_dir");
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    QByteArray path = QDir::toNativeSeparators(calibDir).toUtf8();
    PyObject *ret = PyObject_CallFunction(fn, "s", path.constData());
    Py_DECREF(fn);
    if (!ret) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::measureOd(const uchar *bgr, int h, int w, OdMeasureResult *out, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module || !bgr || !out || h <= 0 || w <= 0) {
        if (error)
            *error = QStringLiteral("invalid measureOd args");
        return false;
    }
    const Py_ssize_t nbytes = static_cast<Py_ssize_t>(h) * w * 3;
    PyObject *bytesObj = PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bgr), nbytes);
    if (!bytesObj) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "measure_od");
    if (!fn) {
        Py_DECREF(bytesObj);
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallFunction(fn, "Oiii", bytesObj, h, w, 3);
    Py_DECREF(fn);
    Py_DECREF(bytesObj);
    if (!ret || !PyDict_Check(ret)) {
        if (error)
            *error = ret ? QStringLiteral("measure_od bad return") : pyErrToString();
        Py_XDECREF(ret);
        return false;
    }
    out->ok = dictGetBool(ret, "ok");
    out->d1Mm = dictGetFloat(ret, "d1_mm");
    out->d2Mm = dictGetFloat(ret, "d2_mm");
    out->score = dictGetFloat(ret, "score");
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::measureLength(const uchar *bgr, int h, int w, LengthMeasureResult *out, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module || !bgr || !out || h <= 0 || w <= 0) {
        if (error)
            *error = QStringLiteral("invalid measureLength args");
        return false;
    }
    const Py_ssize_t nbytes = static_cast<Py_ssize_t>(h) * w * 3;
    PyObject *bytesObj = PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bgr), nbytes);
    if (!bytesObj) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "measure_length");
    if (!fn) {
        Py_DECREF(bytesObj);
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallFunction(fn, "Oiii", bytesObj, h, w, 3);
    Py_DECREF(fn);
    Py_DECREF(bytesObj);
    if (!ret || !PyDict_Check(ret)) {
        if (error)
            *error = ret ? QStringLiteral("measure_length bad return") : pyErrToString();
        Py_XDECREF(ret);
        return false;
    }
    out->ok = dictGetBool(ret, "ok");
    out->lengthMm = dictGetFloat(ret, "length_mm");
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::inspectDefect(const uchar *bgr, int h, int w, DefectMeasureResult *out, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module || !bgr || !out || h <= 0 || w <= 0) {
        if (error)
            *error = QStringLiteral("invalid inspectDefect args");
        return false;
    }
    const Py_ssize_t nbytes = static_cast<Py_ssize_t>(h) * w * 3;
    PyObject *bytesObj = PyBytes_FromStringAndSize(reinterpret_cast<const char *>(bgr), nbytes);
    if (!bytesObj) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "inspect_defect");
    if (!fn) {
        Py_DECREF(bytesObj);
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallFunction(fn, "Oiii", bytesObj, h, w, 3);
    Py_DECREF(fn);
    Py_DECREF(bytesObj);
    if (!ret || !PyDict_Check(ret)) {
        if (error)
            *error = ret ? QStringLiteral("inspect_defect bad return") : pyErrToString();
        Py_XDECREF(ret);
        return false;
    }
    out->ok = dictGetBool(ret, "ok");
    out->grade = static_cast<int>(dictGetLong(ret, "grade"));
    out->defectCount = static_cast<int>(dictGetLong(ret, "defect_count"));
    out->summary = objectToQString(PyDict_GetItemString(ret, "summary"));
    Py_DECREF(ret);
    return true;
}

bool PythonBridge::grabOpen(const QString &source, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module) {
        if (error)
            *error = QStringLiteral("PythonBridge not ready");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "grab_open");
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    QByteArray src = source.toUtf8();
    PyObject *ret = PyObject_CallFunction(fn, "s", src.constData());
    Py_DECREF(fn);
    if (!ret) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    const bool ok = PyObject_IsTrue(ret) == 1;
    Py_DECREF(ret);
    if (!ok && error)
        *error = QStringLiteral("grab_open failed for %1").arg(source);
    return ok;
}

bool PythonBridge::grabRead(QByteArray *bgr, int *h, int *w, QString *error)
{
    PyLock lock;
    if (!m_ready || !m_module || !bgr || !h || !w) {
        if (error)
            *error = QStringLiteral("invalid grabRead args");
        return false;
    }
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "grab_read");
    if (!fn) {
        if (error)
            *error = pyErrToString();
        return false;
    }
    PyObject *ret = PyObject_CallObject(fn, nullptr);
    Py_DECREF(fn);
    if (!ret || ret == Py_None) {
        Py_XDECREF(ret);
        return false; // EOF / fail
    }
    if (!PyTuple_Check(ret) || PyTuple_Size(ret) != 3) {
        Py_DECREF(ret);
        if (error)
            *error = QStringLiteral("grab_read bad return");
        return false;
    }
    PyObject *bytesObj = PyTuple_GetItem(ret, 0);
    PyObject *hObj = PyTuple_GetItem(ret, 1);
    PyObject *wObj = PyTuple_GetItem(ret, 2);
    if (!PyBytes_Check(bytesObj)) {
        Py_DECREF(ret);
        if (error)
            *error = QStringLiteral("grab_read: not bytes");
        return false;
    }
    char *buf = nullptr;
    Py_ssize_t n = 0;
    PyBytes_AsStringAndSize(bytesObj, &buf, &n);
    *bgr = QByteArray(buf, static_cast<int>(n));
    *h = static_cast<int>(PyLong_AsLong(hObj));
    *w = static_cast<int>(PyLong_AsLong(wObj));
    Py_DECREF(ret);
    return true;
}

void PythonBridge::grabClose()
{
    PyLock lock;
    if (!m_ready || !m_module)
        return;
    PyObject *fn = PyObject_GetAttrString(static_cast<PyObject *>(m_module), "grab_close");
    if (!fn) {
        PyErr_Clear();
        return;
    }
    PyObject *ret = PyObject_CallObject(fn, nullptr);
    Py_DECREF(fn);
    Py_XDECREF(ret);
    PyErr_Clear();
}
