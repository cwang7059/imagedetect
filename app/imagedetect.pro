QT += core gui widgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++14
TEMPLATE = app
TARGET = imagedetect

QMAKE_CFLAGS += /utf-8
QMAKE_CXXFLAGS += /utf-8

# ---- Python ----
isEmpty(PYTHON_ROOT) {
    PYTHON_ROOT = $$(PYTHON_ROOT)
}
isEmpty(PYTHON_ROOT) {
    PYTHON_ROOT = C:/Users/rkx/AppData/Local/Python/pythoncore-3.14-64
}
isEmpty(PYTHON_VERSION) {
    PYTHON_VERSION = 314
}

INCLUDEPATH += $$PYTHON_ROOT/Include $$PYTHON_ROOT/include
LIBS += -L$$PYTHON_ROOT/libs -lpython$$PYTHON_VERSION

INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/../third_party/snap7/include
INCLUDEPATH += $$PWD/../third_party/onna/include

# ---- OpenCV ----
isEmpty(OPENCV_DIR) {
    OPENCV_DIR = $$(OPENCV_DIR)
}
isEmpty(OPENCV_DIR) {
    OPENCV_DIR = D:/opencv/opencv/build
}
exists($$OPENCV_DIR/include/opencv2/opencv.hpp) {
    DEFINES += USE_OPENCV
    INCLUDEPATH += $$OPENCV_DIR/include
    LIBS += -L$$OPENCV_DIR/x64/vc16/lib
    CONFIG(debug, debug|release) {
        LIBS += -lopencv_world4100d
    } else {
        LIBS += -lopencv_world4100
    }
    message(OpenCV enabled: $$OPENCV_DIR)
} else {
    message(OpenCV not found at $$OPENCV_DIR)
}

# ---- Hikvision MVS (auto-detect) ----
isEmpty(MVS_DIR) {
    MVS_DIR = $$(MVS_DIR)
}
isEmpty(MVS_DIR) {
    MVS_DIR = C:/Program Files (x86)/MVS/Development
}
exists($$MVS_DIR/Includes/MvCameraControl.h) {
    DEFINES += USE_HIK_MVS
    INCLUDEPATH += $$MVS_DIR/Includes
    LIBS += -L$$MVS_DIR/Libraries/win64 -lMvCameraControl
    message(Hikvision MVS enabled: $$MVS_DIR)
} else {
    message(Hikvision MVS not found — HikLineScanCamera uses simulation)
}

# Snap7: runtime LoadLibrary only (optional SNAP7_DIR for docs/scripts)
message(Snap7: runtime load snap7.dll via Snap7Loader)

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    CaptureWorker.cpp \
    PythonBridge.cpp \
    Overlay.cpp \
    core/Recipe.cpp \
    core/FinalJudge.cpp \
    core/ProductTracker.cpp \
    plc/PlcPointMap.cpp \
    plc/IPlcClient.cpp \
    plc/MockPlcClient.cpp \
    plc/Snap7Loader.cpp \
    plc/S7Client.cpp \
    plc/ModbusTcpClient.cpp \
    camera/SimLineScanCamera.cpp \
    camera/HikLineScanCamera.cpp \
    camera/OnnaLoader.cpp \
    camera/OnnaLineScanCamera.cpp \
    camera/CaptureDispatcher.cpp \
    algo/StationAlgoWorker.cpp \
    algo/AlgoDispatcher.cpp \
    line/ResultTrace.cpp \
    line/LineController.cpp

HEADERS += \
    MainWindow.h \
    CaptureWorker.h \
    PythonBridge.h \
    Overlay.h \
    core/StationTypes.h \
    core/Recipe.h \
    core/ProductRecord.h \
    core/FinalJudge.h \
    core/ProductTracker.h \
    plc/PlcPointMap.h \
    plc/IPlcClient.h \
    plc/MockPlcClient.h \
    plc/Snap7Loader.h \
    plc/S7Client.h \
    plc/ModbusTcpClient.h \
    camera/FramePacket.h \
    camera/ICamera.h \
    camera/SimLineScanCamera.h \
    camera/HikLineScanCamera.h \
    camera/OnnaLoader.h \
    camera/OnnaLineScanCamera.h \
    camera/CaptureDispatcher.h \
    algo/StationAlgoWorker.h \
    algo/AlgoDispatcher.h \
    line/ResultTrace.h \
    line/LineController.h

FORMS += \
    MainWindow.ui
