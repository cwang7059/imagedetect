#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaType>
#include <QPixmap>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(const QString &projectRoot, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_projectRoot(projectRoot)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("雷管高速检测 — Qt + Python"));

    qRegisterMetaType<TrackMeta>("TrackMeta");
    qRegisterMetaType<FramePacket>("FramePacket");
    qRegisterMetaType<ProductRecord>("ProductRecord");
    qRegisterMetaType<StationOdResult>("StationOdResult");
    qRegisterMetaType<StationLengthResult>("StationLengthResult");
    qRegisterMetaType<StationDefectResult>("StationDefectResult");

    connect(ui->btnSynth, &QPushButton::clicked, this, &MainWindow::onSynth);
    connect(ui->btnCamera, &QPushButton::clicked, this, &MainWindow::onCamera);
    connect(ui->btnVideo, &QPushButton::clicked, this, &MainWindow::onVideo);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(ui->comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    connect(ui->sliderThreshold, &QSlider::valueChanged, this, &MainWindow::onThreshold);
    connect(ui->sliderGate, &QSlider::valueChanged, this, &MainWindow::onGate);

    ui->sliderThreshold->setValue(180);
    ui->sliderGate->setValue(80);
    ui->labelThreshold->setText(QStringLiteral("阈值 180"));
    ui->labelGate->setText(QStringLiteral("波门 80"));
    ui->comboMode->setCurrentIndex(0);

    setupLineTab();
    setupWorker();
    // Defer heavy line init (Python ×3 + optional camera SDK) until after first paint.
    QTimer::singleShot(0, this, &MainWindow::initLineController);
}

MainWindow::~MainWindow()
{
    if (m_line) {
        m_line->setLivePreviewEnabled(false);
        m_line->stopLine();
    }
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stopCapture", Qt::QueuedConnection);
    }
    m_thread.quit();
    m_thread.wait(3000);
    delete ui;
}

void MainWindow::setupLineTab()
{
    auto *central = ui->centralwidget;
    auto *oldLayout = central->layout();
    QWidget *sandboxPage = new QWidget(central);
    sandboxPage->setLayout(oldLayout);

    auto *tabs = new QTabWidget(central);
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(tabs);
    tabs->addTab(sandboxPage, QStringLiteral("算法沙盒"));

    auto *linePage = new QWidget(tabs);
    auto *v = new QVBoxLayout(linePage);

    auto *row = new QHBoxLayout();
    auto *btnConnect = new QPushButton(QStringLiteral("连接PLC"), linePage);
    auto *btnLoop = new QPushButton(QStringLiteral("环回测试"), linePage);
    auto *btnSelf = new QPushButton(QStringLiteral("自检"), linePage);
    m_btnLineStart = new QPushButton(QStringLiteral("启动产线"), linePage);
    m_btnLineStop = new QPushButton(QStringLiteral("停止"), linePage);
    m_btnLineStart->setEnabled(false);
    m_btnLineStop->setEnabled(false);
    auto *btnSim = new QPushButton(QStringLiteral("模拟一发"), linePage);
    m_chkLivePreview = new QCheckBox(QStringLiteral("实时预览"), linePage);
    m_chkLivePreview->setChecked(true);
    row->addWidget(btnConnect);
    row->addWidget(btnLoop);
    row->addWidget(btnSelf);
    row->addWidget(m_btnLineStart);
    row->addWidget(m_btnLineStop);
    row->addWidget(btnSim);
    row->addWidget(m_chkLivePreview);
    row->addStretch();
    v->addLayout(row);

    m_linePlc = new QLabel(QStringLiteral("PLC: 未连接"), linePage);
    m_lineStats = new QLabel(QStringLiteral("OK 0 / NG 0"), linePage);
    auto *infoRow = new QHBoxLayout();
    infoRow->addWidget(m_linePlc);
    infoRow->addWidget(m_lineStats);
    infoRow->addStretch();
    v->addLayout(infoRow);

    // 同一客户端内四路画面：外径两端圆形截面 + 长度 + 瑕疵
    auto *camGrid = new QGridLayout();
    auto makeBox = [&](const QString &title, QLabel **outLabel, int row, int col) {
        auto *box = new QGroupBox(title, linePage);
        auto *bl = new QVBoxLayout(box);
        *outLabel = makeStationPreviewLabel(title, box);
        bl->addWidget(*outLabel);
        camGrid->addWidget(box, row, col);
    };
    makeBox(QStringLiteral("外径端 A（D1 · 圆形截面）"), &m_previewOdA, 0, 0);
    makeBox(QStringLiteral("外径端 B（D2 · 圆形截面）"), &m_previewOdB, 0, 1);
    makeBox(QStringLiteral("ST2 长度"), &m_previewLen, 1, 0);
    makeBox(QStringLiteral("ST3 瑕疵"), &m_previewDef, 1, 1);
    camGrid->setRowStretch(0, 1);
    camGrid->setRowStretch(1, 1);
    camGrid->setColumnStretch(0, 1);
    camGrid->setColumnStretch(1, 1);
    v->addLayout(camGrid, 3);

    auto *mid = new QHBoxLayout();
    m_lineTable = new QTableWidget(0, 7, linePage);
    m_lineTable->setHorizontalHeaderLabels(
        {QStringLiteral("槽位"), QStringLiteral("结果"), QStringLiteral("D1"), QStringLiteral("D2"),
         QStringLiteral("L"), QStringLiteral("瑕疵"), QStringLiteral("原因")});
    mid->addWidget(m_lineTable, 1);
    v->addLayout(mid, 1);

    m_lineLog = new QPlainTextEdit(linePage);
    m_lineLog->setReadOnly(true);
    m_lineLog->setMaximumBlockCount(500);
    v->addWidget(m_lineLog);

    tabs->addTab(linePage, QStringLiteral("产线运行"));
    tabs->setCurrentIndex(1);

    m_line = new LineController(this);
    appendLineLog(QStringLiteral("界面已就绪，正在后台初始化产线…"));

    connect(btnConnect, &QPushButton::clicked, this, &MainWindow::onLineConnect);
    connect(btnLoop, &QPushButton::clicked, this, &MainWindow::onLineLoopback);
    connect(btnSelf, &QPushButton::clicked, this, &MainWindow::onLineSelfCheck);
    connect(m_btnLineStart, &QPushButton::clicked, this, &MainWindow::onLineStart);
    connect(m_btnLineStop, &QPushButton::clicked, this, &MainWindow::onLineStop);
    connect(btnSim, &QPushButton::clicked, this, &MainWindow::onLineSimulate);
    connect(m_chkLivePreview, &QCheckBox::toggled, this, &MainWindow::onLivePreviewToggled);

    connect(m_line, &LineController::statusMessage, this, &MainWindow::onLineStatus);
    connect(m_line, &LineController::alarm, this, &MainWindow::onLineAlarm);
    connect(m_line, &LineController::statsChanged, this, &MainWindow::onLineStats);
    connect(m_line, &LineController::recordJudged, this, &MainWindow::onLineJudged);
    connect(m_line, &LineController::previewFrame, this, &MainWindow::onLinePreview);
    connect(m_line, &LineController::plcStateChanged, this, &MainWindow::onPlcState);
}

void MainWindow::initLineController()
{
    if (!m_line)
        return;
    QString err;
    if (!m_line->initialize(m_projectRoot, &err)) {
        appendLineLog(QStringLiteral("初始化失败: %1").arg(err));
        QMessageBox::warning(this, QStringLiteral("产线初始化"), err);
    } else {
        appendLineLog(QStringLiteral("产线模块已就绪（外径两端 / 长度 / 瑕疵 同屏显示）"));
        m_btnLineStart->setEnabled(true);
        m_btnLineStop->setEnabled(true);
        loadOdPreviewCalib();
        if (m_chkLivePreview && m_chkLivePreview->isChecked())
            m_line->setLivePreviewEnabled(true);
    }
}

void MainWindow::loadOdPreviewCalib()
{
    const QString path = m_projectRoot + QStringLiteral("/config/calib/st1_od.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.contains(QStringLiteral("end_a_x_ratio")))
        m_odEndARatio = o.value(QStringLiteral("end_a_x_ratio")).toDouble(0.18);
    if (o.contains(QStringLiteral("end_b_x_ratio")))
        m_odEndBRatio = o.value(QStringLiteral("end_b_x_ratio")).toDouble(0.82);
    const int band = o.value(QStringLiteral("band_half_width_px")).toInt(24);
    m_odCropHalfWidth = qMax(32, band * 2);
}

QLabel *MainWindow::makeStationPreviewLabel(const QString &title, QWidget *parent)
{
    auto *lab = new QLabel(QStringLiteral("等待 %1 …").arg(title), parent);
    lab->setMinimumSize(320, 200);
    lab->setAlignment(Qt::AlignCenter);
    lab->setFrameShape(QFrame::Box);
    lab->setStyleSheet(QStringLiteral("background:#1a1a1a; color:#888;"));
    return lab;
}

void MainWindow::showImageOnLabel(QLabel *label, const QImage &img, bool isBgr)
{
    if (!label || img.isNull())
        return;
    QImage rgb = isBgr ? img.rgbSwapped() : img;
    label->setPixmap(QPixmap::fromImage(rgb).scaled(
        label->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

void MainWindow::showOdEndCrops(const FramePacket &frame)
{
    if (frame.buffer.isEmpty() || frame.width <= 0 || frame.height <= 0)
        return;

    QImage full(reinterpret_cast<const uchar *>(frame.buffer.constData()),
                frame.width,
                frame.height,
                frame.width * qMax(1, frame.channels),
                frame.channels >= 3 ? QImage::Format_RGB888 : QImage::Format_Grayscale8);

    const int side = qBound(64, qMin(frame.height, m_odCropHalfWidth * 2), frame.width);
    auto cropEnd = [&](double ratio) -> QImage {
        const int cx = qBound(0, int(ratio * frame.width), frame.width - 1);
        const int x0 = qBound(0, cx - side / 2, frame.width - side);
        const int y0 = qBound(0, (frame.height - side) / 2, frame.height - side);
        return full.copy(x0, y0, side, side);
    };

    // 两端圆形截面：按标定 end_a / end_b 裁切为正方形小窗
    showImageOnLabel(m_previewOdA, cropEnd(m_odEndARatio), frame.channels >= 3);
    showImageOnLabel(m_previewOdB, cropEnd(m_odEndBRatio), frame.channels >= 3);
}

void MainWindow::appendLineLog(const QString &msg)
{
    if (m_lineLog)
        m_lineLog->appendPlainText(msg);
    statusBar()->showMessage(msg, 4000);
}

void MainWindow::setupWorker()
{
    m_worker = new CaptureWorker();
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &CaptureWorker::frameReady, this, &MainWindow::onFrame);
    connect(m_worker, &CaptureWorker::statusMessage, this, &MainWindow::onStatus);
    connect(m_worker, &CaptureWorker::failed, this, &MainWindow::onFailed);
    connect(m_worker, &CaptureWorker::prepared, this, &MainWindow::onPrepared);
    connect(m_worker, &CaptureWorker::sourceChanged, this, &MainWindow::onSourceChanged);

    m_thread.start();

    QMetaObject::invokeMethod(
        m_worker,
        "prepare",
        Qt::QueuedConnection,
        Q_ARG(QString, m_projectRoot),
        Q_ARG(int, ui->sliderThreshold->value()),
        Q_ARG(int, ui->sliderGate->value()));
}

void MainWindow::onPrepared()
{
    QMetaObject::invokeMethod(m_worker, "setSourceSynth", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection);
    statusBar()->showMessage(QStringLiteral("沙盒运行中 · 合成源"));
}

void MainWindow::onFrame(const QImage &vis, const QImage &mask, const TrackMeta &meta, double fps)
{
    ui->labelVis->setPixmap(QPixmap::fromImage(vis).scaled(
        ui->labelVis->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    ui->labelMask->setPixmap(QPixmap::fromImage(mask).scaled(
        ui->labelMask->size(), Qt::KeepAspectRatio, Qt::FastTransformation));

    const QString miss = meta.locked
        ? QStringLiteral("脱靶 (%1, %2)").arg(meta.missX, 0, 'f', 1).arg(meta.missY, 0, 'f', 1)
        : QStringLiteral("搜索中");
    const QString modeTag = (meta.mode == QLatin1String("face"))
        ? QStringLiteral("人脸")
        : QStringLiteral("亮斑");
    statusBar()->showMessage(
        QStringLiteral("%1 | %2 | %3 | FPS %4 | %5 | %6")
            .arg(m_sourceLabel)
            .arg(modeTag)
            .arg(meta.locked ? QStringLiteral("LOCK") : QStringLiteral("SEARCH"))
            .arg(fps, 0, 'f', 1)
            .arg(miss)
            .arg(meta.backend));
}

void MainWindow::onSourceChanged(const QString &label)
{
    m_sourceLabel = label;
    ui->btnCamera->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("当前源: %1").arg(label), 2000);
}

void MainWindow::onStatus(const QString &msg)
{
    statusBar()->showMessage(msg, 3000);
}

void MainWindow::onFailed(const QString &err)
{
    QMessageBox::critical(this, QStringLiteral("初始化失败"), err);
}

void MainWindow::onSynth()
{
    QMetaObject::invokeMethod(m_worker, "setSourceSynth", Qt::QueuedConnection);
}

void MainWindow::onCamera()
{
    statusBar()->showMessage(QStringLiteral("正在打开摄像头…（最多约 9 秒，勿重复点击）"));
    ui->btnCamera->setEnabled(false);
    QTimer::singleShot(10000, this, [this]() { ui->btnCamera->setEnabled(true); });
    QMetaObject::invokeMethod(
        m_worker, "setSourceCamera", Qt::QueuedConnection, Q_ARG(int, ui->spinCamera->value()));
}

void MainWindow::onVideo()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择视频"),
        QString(),
        QStringLiteral("Video (*.mp4 *.avi *.mkv *.mov);;All (*.*)"));
    if (path.isEmpty())
        return;
    QMetaObject::invokeMethod(
        m_worker, "setSourceVideo", Qt::QueuedConnection, Q_ARG(QString, path));
}

void MainWindow::onReset()
{
    QMetaObject::invokeMethod(m_worker, "resetTrack", Qt::QueuedConnection);
}

void MainWindow::onModeChanged(int index)
{
    const QString mode = (index == 1) ? QStringLiteral("face") : QStringLiteral("bright");
    QMetaObject::invokeMethod(
        m_worker, "setDetectMode", Qt::QueuedConnection, Q_ARG(QString, mode));
    if (index == 1 && ui->sliderGate->value() < 160) {
        ui->sliderGate->setValue(160);
    }
    ui->sliderThreshold->setEnabled(index == 0);
}

void MainWindow::onThreshold(int v)
{
    ui->labelThreshold->setText(QStringLiteral("阈值 %1").arg(v));
    QMetaObject::invokeMethod(m_worker, "setThreshold", Qt::QueuedConnection, Q_ARG(int, v));
}

void MainWindow::onGate(int v)
{
    ui->labelGate->setText(QStringLiteral("波门 %1").arg(v));
    QMetaObject::invokeMethod(m_worker, "setGate", Qt::QueuedConnection, Q_ARG(int, v));
}

void MainWindow::onLineConnect()
{
    if (!m_line)
        return;
    QString err;
    if (!m_line->connectPlc(&err)) {
        QMessageBox::warning(this, QStringLiteral("PLC"), err);
        appendLineLog(err);
    }
}

void MainWindow::onLineStart()
{
    if (m_line)
        m_line->startLine();
}

void MainWindow::onLineStop()
{
    if (m_line)
        m_line->stopLine();
}

void MainWindow::onLineSelfCheck()
{
    if (!m_line)
        return;
    QString err;
    if (!m_line->runSelfCheck(&err)) {
        QMessageBox::warning(this, QStringLiteral("自检失败"), err);
        appendLineLog(err);
    }
}

void MainWindow::onLineSimulate()
{
    if (!m_line)
        return;
    m_line->simulateOneCycle(static_cast<quint16>(m_simSlot));
    m_simSlot = (m_simSlot % 200) + 1;
}

void MainWindow::onLineLoopback()
{
    if (!m_line)
        return;
    QString err;
    if (!m_line->plcConnected()) {
        if (!m_line->connectPlc(&err)) {
            QMessageBox::warning(this, QStringLiteral("环回"), err);
            return;
        }
    }
    if (m_line->runLoopbackTest(&err))
        appendLineLog(QStringLiteral("PLC 环回成功"));
    else
        QMessageBox::warning(this, QStringLiteral("环回失败"), err);
}

void MainWindow::onLineStatus(const QString &msg)
{
    appendLineLog(msg);
}

void MainWindow::onLineAlarm(const QString &msg)
{
    appendLineLog(QStringLiteral("[ALARM] %1").arg(msg));
}

void MainWindow::onLineStats(int ok, int ng)
{
    if (m_lineStats)
        m_lineStats->setText(QStringLiteral("OK %1 / NG %2").arg(ok).arg(ng));
}

void MainWindow::onPlcState(bool connected, const QString &detail)
{
    if (m_linePlc)
        m_linePlc->setText(QStringLiteral("PLC: %1 (%2)")
                               .arg(connected ? QStringLiteral("已连接") : QStringLiteral("断开"))
                               .arg(detail));
}

void MainWindow::onLineJudged(const ProductRecord &rec)
{
    if (!m_lineTable)
        return;
    const int row = m_lineTable->rowCount();
    m_lineTable->insertRow(row);
    m_lineTable->setItem(row, 0, new QTableWidgetItem(QString::number(rec.slot)));
    m_lineTable->setItem(row, 1, new QTableWidgetItem(rec.pass ? QStringLiteral("PASS") : QStringLiteral("NG")));
    m_lineTable->setItem(row, 2, new QTableWidgetItem(QString::number(rec.od.d1Mm, 'f', 3)));
    m_lineTable->setItem(row, 3, new QTableWidgetItem(QString::number(rec.od.d2Mm, 'f', 3)));
    m_lineTable->setItem(row, 4, new QTableWidgetItem(QString::number(rec.length.lengthMm, 'f', 3)));
    m_lineTable->setItem(row, 5, new QTableWidgetItem(QString::number(rec.defect.grade)));
    m_lineTable->setItem(row, 6, new QTableWidgetItem(rec.failReason));
    m_lineTable->scrollToBottom();
}

void MainWindow::onLinePreview(const FramePacket &frame)
{
    switch (frame.station) {
    case StationId::ST1_Od:
        showOdEndCrops(frame);
        break;
    case StationId::ST2_Length: {
        QImage img(reinterpret_cast<const uchar *>(frame.buffer.constData()),
                   frame.width,
                   frame.height,
                   frame.width * qMax(1, frame.channels),
                   frame.channels >= 3 ? QImage::Format_RGB888 : QImage::Format_Grayscale8);
        showImageOnLabel(m_previewLen, img, frame.channels >= 3);
        break;
    }
    case StationId::ST3_Defect: {
        QImage img(reinterpret_cast<const uchar *>(frame.buffer.constData()),
                   frame.width,
                   frame.height,
                   frame.width * qMax(1, frame.channels),
                   frame.channels >= 3 ? QImage::Format_RGB888 : QImage::Format_Grayscale8);
        showImageOnLabel(m_previewDef, img, frame.channels >= 3);
        break;
    }
    default:
        break;
    }
}

void MainWindow::onLivePreviewToggled(bool on)
{
    if (m_line)
        m_line->setLivePreviewEnabled(on);
}
