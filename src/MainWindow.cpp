#include "MainWindow.h"
#include "VideoPlayer.h"
#include "VideoWidget.h"
#include "FrameProcessor.h"
#include "TrajectoryWidget.h"
#include "StatisticsWidget.h"
#include "AlertManager.h"
#include "BehaviorAnalyzer.h"
#include "DetectionResult.h"
#include "DatabaseManager.h"
#include "HistoryDialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QStatusBar>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QFrame>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QTimer>
#include <opencv2/core.hpp>

static constexpr int CHART_UPDATE_INTERVAL_MS = 500;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("宠物居家场景可视化系统"));
    resize(1480, 900);

    m_videoPlayer      = new VideoPlayer(this);
    m_videoWidget      = new VideoWidget(this);
    m_frameProcessor   = new FrameProcessor(nullptr);
    m_trajectoryWidget = new TrajectoryWidget(this);
    m_statisticsWidget = new StatisticsWidget(this);
    m_alertManager     = new AlertManager(this);

    DatabaseManager::instance().open();

    setupUi();
    applyTheme();
    connectSignals();
    loadModel();

    m_displayTimer.start();
    m_chartUpdateTimer.start();

    QTimer::singleShot(0, this, &MainWindow::animateStartup);
}

MainWindow::~MainWindow()
{
    if (m_currentSessionId > 0) {
        DatabaseManager::instance().saveBehaviorStats(m_currentSessionId, m_pendingDurations);
        DatabaseManager::instance().closeSession(m_currentSessionId);
    }
    m_frameProcessor->stop();
    delete m_frameProcessor;
    DatabaseManager::instance().close();
}

void MainWindow::loadModel()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/models/yolov8n.onnx",
        QCoreApplication::applicationDirPath() + "/../models/yolov8n.onnx",
        QDir::currentPath() + "/models/yolov8n.onnx",
        "D:/code/Graduation Project/PetMonitorSystem/models/yolov8n.onnx",
    };

    bool modelLoaded = false;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            if (m_frameProcessor->loadModel(path)) {
                statusBar()->showMessage(QStringLiteral("模型加载成功，请打开视频或摄像头"));
                modelLoaded = true;
                break;
            }
        }
    }

    if (!modelLoaded) {
        QString msg = QStringLiteral("警告：未找到YOLO模型文件，已搜索路径：\n");
        for (const QString &p : searchPaths) msg += p + "\n";
        statusBar()->showMessage(QStringLiteral("警告：未找到YOLO模型文件，请将 yolov8n.onnx 放入 models/ 目录"));
        QMessageBox::warning(this, QStringLiteral("模型未找到"), msg);
    }

    m_frameProcessor->start();
}

void MainWindow::applyTheme()
{
    QFile f(QStringLiteral(":/styles/styles.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}

void MainWindow::animateStartup()
{
    QWidget *cw = centralWidget();
    if (!cw)
        return;
    auto *eff = qobject_cast<QGraphicsOpacityEffect *>(cw->graphicsEffect());
    if (!eff) {
        eff = new QGraphicsOpacityEffect(cw);
        cw->setGraphicsEffect(eff);
        eff->setOpacity(0.0);
    }
    auto *anim = new QPropertyAnimation(eff, "opacity", this);
    anim->setDuration(520);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [cw]() {
        if (cw && cw->graphicsEffect())
            cw->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("centralArea"));
    setCentralWidget(centralWidget);

    m_videoWidget->setMinimumSize(640, 480);

    /* 顶部标题栏 */
    auto *titleBar = new QFrame(centralWidget);
    titleBar->setObjectName(QStringLiteral("titleBar"));
    titleBar->setFixedHeight(52);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 0, 20, 0);
    auto *titleMain = new QLabel(QStringLiteral("宠物居家场景可视化系统"), titleBar);
    titleMain->setObjectName(QStringLiteral("titleMain"));
    auto *titleSub = new QLabel(QStringLiteral("YOLO 检测 · 行为分析 · 轨迹与统计"), titleBar);
    titleSub->setObjectName(QStringLiteral("titleSub"));
    auto *titleTexts = new QVBoxLayout;
    titleTexts->setSpacing(2);
    titleTexts->addWidget(titleMain);
    titleTexts->addWidget(titleSub);
    titleLayout->addLayout(titleTexts);
    titleLayout->addStretch();

    /* 视频区卡片 + 阴影 */
    m_videoFrame = new QFrame(centralWidget);
    m_videoFrame->setObjectName(QStringLiteral("videoFrame"));
    auto *vfLayout = new QVBoxLayout(m_videoFrame);
    vfLayout->setContentsMargins(10, 10, 10, 10);
    vfLayout->addWidget(m_videoWidget);
    // 不在视频区域使用 QGraphicsDropShadowEffect：每帧重绘时开销极大，会导致播放卡顿

    auto *controlGroup = new QGroupBox(QStringLiteral("控制面板"));
    auto *controlLayout = new QVBoxLayout(controlGroup);

    auto *btnLayout = new QHBoxLayout;
    m_btnOpen = new QPushButton(QStringLiteral("打开视频"));
    m_btnCamera = new QPushButton(QStringLiteral("打开摄像头"));
    m_btnPlay = new QPushButton(QStringLiteral("播放"));
    m_btnPlay->setEnabled(false);
    m_btnOpen->setObjectName(QStringLiteral("btnSecondary"));
    m_btnCamera->setObjectName(QStringLiteral("btnSecondary"));
    m_btnPlay->setObjectName(QStringLiteral("btnPrimary"));
    btnLayout->addWidget(m_btnOpen);
    btnLayout->addWidget(m_btnCamera);
    btnLayout->addWidget(m_btnPlay);
    controlLayout->addLayout(btnLayout);

    auto *camRow = new QHBoxLayout;
    m_btnStartAnalysis = new QPushButton(QStringLiteral("开始分析"));
    m_btnStopAnalysis  = new QPushButton(QStringLiteral("结束分析"));
    m_btnStartAnalysis->setEnabled(false);
    m_btnStopAnalysis->setEnabled(false);
    m_btnStartAnalysis->setObjectName(QStringLiteral("btnSuccess"));
    m_btnStopAnalysis->setObjectName(QStringLiteral("btnWarning"));
    m_btnStartAnalysis->setToolTip(QStringLiteral("摄像头预览时：仅在此区间内进行推理与时段统计"));
    m_btnStopAnalysis->setToolTip(QStringLiteral("结束本段直播分析，可导出 CSV"));
    camRow->addWidget(m_btnStartAnalysis);
    camRow->addWidget(m_btnStopAnalysis);
    controlLayout->addLayout(camRow);

    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setEnabled(false);
    m_slider->setToolTip(QStringLiteral("仅本地视频可拖动进度"));
    controlLayout->addWidget(m_slider);

    m_fpsLabel = new QLabel(QStringLiteral("FPS: --"));
    m_fpsLabel->setObjectName(QStringLiteral("fpsLabel"));
    controlLayout->addWidget(m_fpsLabel);

    auto *segRow = new QHBoxLayout;
    auto *lblMinSeg = new QLabel(QStringLiteral("最短连续出现(秒)："));
    lblMinSeg->setObjectName(QStringLiteral("lblMinSegSec"));
    lblMinSeg->setStyleSheet(QStringLiteral("color: #ffffff;"));
    segRow->addWidget(lblMinSeg);
    m_spinMinSegSec = new QDoubleSpinBox;
    m_spinMinSegSec->setRange(0.1, 30.0);
    m_spinMinSegSec->setValue(0.5);
    m_spinMinSegSec->setSingleStep(0.1);
    m_spinMinSegSec->setDecimals(2);
    m_spinMinSegSec->setToolTip(QStringLiteral("仅保留连续出现超过此时长的区段"));
    segRow->addWidget(m_spinMinSegSec);
    segRow->addStretch();
    controlLayout->addLayout(segRow);

    auto *scanRow = new QHBoxLayout;
    auto *lblFullScanStride = new QLabel(QStringLiteral("整段统计 步长:"));
    lblFullScanStride->setObjectName(QStringLiteral("lblFullScanStride"));
    lblFullScanStride->setStyleSheet(QStringLiteral("color: #ffffff;"));
    scanRow->addWidget(lblFullScanStride);
    m_spinFullScanStride = new QSpinBox;
    m_spinFullScanStride->setRange(1, 30);
    m_spinFullScanStride->setValue(3);
    m_spinFullScanStride->setToolTip(
        QStringLiteral("每 N 帧做一次 YOLO；中间帧用 grab 快速跳过解码，并沿用本段检测结果。"
                       "数值越大越快，极短出现可能被漏检。设为 1 则逐帧推理（最慢最准）。"));
    scanRow->addWidget(m_spinFullScanStride);
    scanRow->addStretch();
    controlLayout->addLayout(scanRow);

    auto *inferRow = new QHBoxLayout;
    auto *lblRealtimeInferStride = new QLabel(QStringLiteral("实时推理步长:"));
    lblRealtimeInferStride->setObjectName(QStringLiteral("lblRealtimeInferStride"));
    lblRealtimeInferStride->setStyleSheet(QStringLiteral("color: #ffffff;"));
    inferRow->addWidget(lblRealtimeInferStride);
    m_spinRealtimeInferStride = new QSpinBox;
    m_spinRealtimeInferStride->setRange(1, 8);
    m_spinRealtimeInferStride->setValue(1);
    m_spinRealtimeInferStride->setToolTip(
        QStringLiteral("视频仍按原 FPS 全速播放；仅每 N 帧尝试提交一次 YOLO。"
                       "若推理慢于播放，还会自动跳过提交，检测框可能略滞后。"));
    inferRow->addWidget(m_spinRealtimeInferStride);
    inferRow->addStretch();
    controlLayout->addLayout(inferRow);

    m_btnGenerateWholeStats = new QPushButton(QStringLiteral("整段统计(时段表)"));
    m_btnGenerateWholeStats->setEnabled(false);
    m_btnGenerateWholeStats->setObjectName(QStringLiteral("btnAccent"));
    m_btnGenerateWholeStats->setToolTip(
        QStringLiteral("后台扫描整段视频，仅用于生成「宠物出现时段」CSV；可调步长加速（推理分辨率与模型一致）"));
    controlLayout->addWidget(m_btnGenerateWholeStats);

    m_btnExport = new QPushButton(QStringLiteral("导出宠物出现时段(CSV)"));
    m_btnExport->setEnabled(false);
    m_btnExport->setObjectName(QStringLiteral("btnPrimary"));
    m_btnExport->setToolTip(QStringLiteral("本地视频：需先完成「整段统计」；摄像头：结束分析后"));
    controlLayout->addWidget(m_btnExport);

    m_btnHistory = new QPushButton(QStringLiteral("历史数据"));
    m_btnHistory->setObjectName(QStringLiteral("btnSecondary"));
    m_btnHistory->setToolTip(QStringLiteral("查看历史分析会话的行为统计、告警记录与出现时段"));
    controlLayout->addWidget(m_btnHistory);

    auto *trajGroup = new QGroupBox(QStringLiteral("运动轨迹"));
    auto *trajLayout = new QVBoxLayout(trajGroup);
    m_trajectoryWidget->setMinimumHeight(200);
    trajLayout->addWidget(m_trajectoryWidget);

    auto *statsGroup = new QGroupBox(QStringLiteral("行为统计"));
    auto *statsLayout = new QVBoxLayout(statsGroup);
    m_statisticsWidget->setMinimumHeight(250);
    statsLayout->addWidget(m_statisticsWidget);

    auto *rightPanel = new QFrame(centralWidget);
    rightPanel->setObjectName(QStringLiteral("sidePanel"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->setSpacing(10);
    rightLayout->addWidget(controlGroup);
    rightLayout->addWidget(trajGroup, 1);
    rightLayout->addWidget(statsGroup, 1);
    rightPanel->setFixedWidth(430);
    auto *sideShadow = new QGraphicsDropShadowEffect(rightPanel);
    sideShadow->setBlurRadius(22);
    sideShadow->setColor(QColor(0, 0, 0, 70));
    sideShadow->setOffset(-4, 4);
    rightPanel->setGraphicsEffect(sideShadow);

    m_alertManager->setFixedHeight(0);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);
    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(m_alertManager);

    m_mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);
    m_mainSplitter->addWidget(m_videoFrame);
    m_mainSplitter->addWidget(rightPanel);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setChildrenCollapsible(false);
    mainLayout->addWidget(m_mainSplitter, 1);

    m_statusLabel = new QLabel(QStringLiteral("就绪"));
    statusBar()->addPermanentWidget(m_statusLabel);

    /* 启动淡入：先透明，避免首帧闪屏 */
    {
        auto *fadeInit = new QGraphicsOpacityEffect(centralWidget);
        centralWidget->setGraphicsEffect(fadeInit);
        fadeInit->setOpacity(0.0);
    }
}

void MainWindow::connectSignals()
{
    connect(m_btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(m_btnCamera, &QPushButton::clicked, this, &MainWindow::onOpenCamera);
    connect(m_btnPlay, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_btnGenerateWholeStats, &QPushButton::clicked, this,
            &MainWindow::onGenerateWholeTableStats);
    connect(m_btnExport, &QPushButton::clicked, this, &MainWindow::onExportPresenceCsv);
    connect(m_btnHistory, &QPushButton::clicked, this, &MainWindow::onShowHistory);
    connect(m_btnStartAnalysis, &QPushButton::clicked, this, &MainWindow::onStartCameraAnalysis);
    connect(m_btnStopAnalysis, &QPushButton::clicked, this, &MainWindow::onStopCameraAnalysis);
    connect(m_slider, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);
    connect(m_videoPlayer, &VideoPlayer::frameReady, this, &MainWindow::onFrameReady);

    connect(m_videoPlayer, &VideoPlayer::playbackFinished, this, [this]() {
        m_isPlaying = false;
        m_btnPlay->setText(QStringLiteral("播放"));
        m_statusLabel->setText(QStringLiteral("播放完毕"));
    });

    connect(m_frameProcessor, &FrameProcessor::resultsReady,
            this, &MainWindow::onDetectionResults, Qt::QueuedConnection);
    connect(m_frameProcessor, &FrameProcessor::alertCheck,
            this, &MainWindow::onAlertCheck, Qt::QueuedConnection);

    connect(m_frameProcessor, &FrameProcessor::fullVideoProgress,
            this, &MainWindow::onFullVideoProgress, Qt::QueuedConnection);
    connect(m_frameProcessor, &FrameProcessor::fullVideoStatsBlock,
            this, &MainWindow::onFullVideoStatsBlock, Qt::QueuedConnection);
    connect(m_frameProcessor, &FrameProcessor::fullVideoFinished,
            this, &MainWindow::onFullVideoFinished, Qt::QueuedConnection);
    connect(m_frameProcessor, &FrameProcessor::fullVideoFailed,
            this, &MainWindow::onFullVideoFailed, Qt::QueuedConnection);

    connect(m_alertManager, &AlertManager::alertTriggered, this, [this](const QString &msg) {
        if (m_currentSessionId > 0)
            DatabaseManager::instance().saveAlert(m_currentSessionId, msg, QStringLiteral("warning"));
    });
}

void MainWindow::resetPlaybackState()
{
    m_lastDetections.clear();
    m_lastTracks.clear();
    m_chartDataPending = false;
    m_frameProcessor->requestReset();
    m_trajectoryWidget->clearTrajectories();
    m_statisticsWidget->resetStatistics();
    m_alertManager->clearAlerts();
    m_frameCount = 0;
    m_displayTimer.restart();
    m_chartUpdateTimer.restart();
    m_realtimeInferFrameCounter = 0;

    m_presenceRecorder.reset();
    m_presenceRecorder.setFps(m_videoPlayer->fps());
    m_presenceRecorder.setMinSegmentSeconds(m_spinMinSegSec->value());
}

void MainWindow::onOpenFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择视频文件"), QString(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov *.flv);;所有文件 (*)")
    );
    if (filePath.isEmpty()) return;

    if (m_isPlaying) {
        m_videoPlayer->pause();
        m_isPlaying = false;
    }

    if (!m_videoPlayer->openFile(filePath)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法打开视频文件：%1").arg(filePath));
        return;
    }

    if (m_currentSessionId > 0) {
        DatabaseManager::instance().saveBehaviorStats(m_currentSessionId, m_pendingDurations);
        DatabaseManager::instance().closeSession(m_currentSessionId);
    }

    m_isVideoFileMode = true;
    m_currentVideoFilePath = filePath;
    m_cameraAnalyzing = false;
    m_cameraExportReady = false;

    m_currentSessionId = DatabaseManager::instance().createSession(
        filePath, m_videoPlayer->fps(), m_videoPlayer->totalFrames());
    m_btnStartAnalysis->setEnabled(false);
    m_btnStopAnalysis->setEnabled(false);

    const int tf = m_videoPlayer->totalFrames();
    if (tf > 0) {
        m_slider->setRange(0, tf - 1);
        m_slider->setEnabled(true);
    } else {
        m_slider->setRange(0, 0);
        m_slider->setEnabled(false);
    }
    m_btnPlay->setEnabled(true);
    m_btnPlay->setText(QStringLiteral("播放"));
    m_btnGenerateWholeStats->setEnabled(true);
    m_btnExport->setEnabled(false);

    resetPlaybackState();

    m_fileBatchRunning = false;
    m_statusLabel->setText(
        QStringLiteral("已打开本地视频：可播放进行实时检测；需要时段表时点「整段统计」"));
}

void MainWindow::onFullVideoProgress(int currentFrame, int totalFrames)
{
    if (m_fullVideoProgressDlg) {
        m_fullVideoProgressDlg->setMaximum(qMax(1, totalFrames));
        m_fullVideoProgressDlg->setValue(currentFrame);
    }
}

void MainWindow::onFullVideoStatsBlock(int startFrame, int endFrame, bool petPresent)
{
    if (m_isVideoFileMode && m_fileBatchRunning)
        m_presenceRecorder.recordFrameRange(startFrame, endFrame, petPresent);
}

void MainWindow::restoreVideoPlayerAfterFullScan()
{
    if (!m_isVideoFileMode || m_currentVideoFilePath.isEmpty())
        return;
    if (m_videoPlayer->isLiveCamera())
        return;

    if (!m_videoPlayer->openFile(m_currentVideoFilePath)) {
        QMessageBox::warning(
            this,
            QStringLiteral("无法恢复预览"),
            QStringLiteral("整段统计已结束，但重新打开视频失败，请手动再次「打开视频」。"));
        return;
    }
    const int tf = m_videoPlayer->totalFrames();
    if (tf > 0) {
        m_slider->setRange(0, tf - 1);
        const int pos = qBound(0, m_savedFramePosBeforeScan, tf - 1);
        m_slider->setValue(pos);
        m_videoPlayer->seekTo(pos);
    }
}

void MainWindow::onGenerateWholeTableStats()
{
    if (!m_isVideoFileMode || m_currentVideoFilePath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先打开本地视频文件。"));
        return;
    }

    if (m_isPlaying) {
        m_videoPlayer->pause();
        m_isPlaying = false;
        m_btnPlay->setText(QStringLiteral("播放"));
    }

    // 先保存元数据与进度，再释放占用：否则整段统计无法二次打开同一文件（Windows 下常见）
    const int tf = m_videoPlayer->totalFrames();
    const double vidFps = m_videoPlayer->fps();
    m_savedFramePosBeforeScan = m_slider->value();
    m_videoPlayer->releaseCaptureForFullScan();

    m_frameProcessor->requestReset();

    m_presenceRecorder.reset();
    m_presenceRecorder.setFps(vidFps);
    m_presenceRecorder.setMinSegmentSeconds(m_spinMinSegSec->value());

    m_fileBatchRunning = true;
    m_btnGenerateWholeStats->setEnabled(false);
    m_btnExport->setEnabled(false);

    m_statusLabel->setText(QStringLiteral("正在后台整段扫描（仅用于时段表），请稍候…"));

    m_fullVideoProgressDlg = new QProgressDialog(
        QStringLiteral("正在整段扫描（生成时段表数据）…"), QString(), 0, tf > 0 ? tf : 100, this);
    m_fullVideoProgressDlg->setWindowModality(Qt::WindowModal);
    m_fullVideoProgressDlg->setMinimumDuration(0);
    m_fullVideoProgressDlg->setValue(0);
    m_fullVideoProgressDlg->show();

    m_frameProcessor->setFullVideoScanStride(m_spinFullScanStride->value());
    m_frameProcessor->requestFullVideoAnalysis(m_currentVideoFilePath);
}

void MainWindow::onFullVideoFinished()
{
    m_fileBatchRunning = false;
    if (m_fullVideoProgressDlg) {
        m_fullVideoProgressDlg->close();
        m_fullVideoProgressDlg.clear();
    }

    m_presenceRecorder.finalize();

    if (m_currentSessionId > 0)
        DatabaseManager::instance().savePresenceSegments(
            m_currentSessionId, m_presenceRecorder.segments());

    restoreVideoPlayerAfterFullScan();

    if (m_isVideoFileMode) {
        m_btnGenerateWholeStats->setEnabled(true);
        m_btnExport->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("整段时段统计完成，可导出 CSV；播放仍为实时检测模式"));
    }
}

void MainWindow::onFullVideoFailed(const QString &reason)
{
    m_fileBatchRunning = false;
    if (m_isVideoFileMode) {
        m_btnGenerateWholeStats->setEnabled(true);
        m_btnExport->setEnabled(false);
    }
    if (m_fullVideoProgressDlg) {
        m_fullVideoProgressDlg->close();
        m_fullVideoProgressDlg.clear();
    }

    restoreVideoPlayerAfterFullScan();

    QMessageBox::warning(this, QStringLiteral("整段统计失败"), reason);
}

void MainWindow::onOpenCamera()
{
    bool ok = false;
    int deviceIndex = QInputDialog::getInt(
        this,
        QStringLiteral("摄像头设备"),
        QStringLiteral("设备编号（通常为 0，多摄像头可试 1、2…）:"),
        0, 0, 10, 1, &ok);
    if (!ok) return;

    if (m_isPlaying) {
        m_videoPlayer->pause();
        m_isPlaying = false;
    }

    if (!m_videoPlayer->openCamera(deviceIndex)) {
        QMessageBox::warning(
            this,
            QStringLiteral("无法打开摄像头"),
            QStringLiteral("请检查摄像头与隐私设置，或更换设备编号。"));
        return;
    }

    if (m_currentSessionId > 0) {
        DatabaseManager::instance().saveBehaviorStats(m_currentSessionId, m_pendingDurations);
        DatabaseManager::instance().closeSession(m_currentSessionId);
    }

    m_isVideoFileMode = false;
    m_currentVideoFilePath.clear();
    m_fileBatchRunning = false;
    m_cameraAnalyzing = false;
    m_cameraExportReady = false;
    m_btnExport->setEnabled(false);

    m_currentSessionId = DatabaseManager::instance().createSession(
        QStringLiteral("camera:%1").arg(deviceIndex),
        m_videoPlayer->fps(), 0);

    m_btnStartAnalysis->setEnabled(true);
    m_btnStopAnalysis->setEnabled(false);

    m_slider->setRange(0, 0);
    m_slider->setValue(0);
    m_slider->setEnabled(false);
    m_btnPlay->setEnabled(true);
    m_btnPlay->setText(QStringLiteral("播放"));
    m_btnGenerateWholeStats->setEnabled(false);

    resetPlaybackState();

    m_statusLabel->setText(
        QStringLiteral("摄像头 设备%1 — 点「播放」预览；点「开始分析」～「结束分析」统计本段直播")
            .arg(deviceIndex));
}

void MainWindow::onStartCameraAnalysis()
{
    if (!m_videoPlayer->isLiveCamera()) return;

    m_cameraAnalyzing = true;
    m_cameraExportReady = false;
    m_btnExport->setEnabled(false);
    m_btnStartAnalysis->setEnabled(false);
    m_btnStopAnalysis->setEnabled(true);

    resetPlaybackState();
    m_statusLabel->setText(QStringLiteral("正在采集本段直播数据…点「结束分析」停止并汇总"));
}

void MainWindow::onStopCameraAnalysis()
{
    if (!m_videoPlayer->isLiveCamera()) return;

    m_cameraAnalyzing = false;
    m_cameraExportReady = true;
    m_btnStartAnalysis->setEnabled(true);
    m_btnStopAnalysis->setEnabled(false);

    m_presenceRecorder.finalize();

    if (m_currentSessionId > 0) {
        DatabaseManager::instance().savePresenceSegments(
            m_currentSessionId, m_presenceRecorder.segments());
        DatabaseManager::instance().saveBehaviorStats(
            m_currentSessionId, m_pendingDurations);
    }

    m_trajectoryWidget->updateTrajectories(m_lastTracks, m_videoWidth, m_videoHeight);
    m_statisticsWidget->updateStatistics(m_pendingDurations, m_pendingTimeline);
    m_btnExport->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("本段直播分析已结束，可导出 CSV"));
}

void MainWindow::onPlayPause()
{
    if (m_isPlaying) {
        m_videoPlayer->pause();
        m_btnPlay->setText(m_videoPlayer->isLiveCamera()
                               ? QStringLiteral("播放")
                               : QStringLiteral("播放"));
    } else {
        m_videoPlayer->play();
        m_btnPlay->setText(QStringLiteral("暂停"));
    }
    m_isPlaying = !m_isPlaying;
}

void MainWindow::onFrameReady(const cv::Mat &frame)
{
    m_videoWidth  = frame.cols;
    m_videoHeight = frame.rows;

    m_videoWidget->updateFrame(frame, m_lastDetections);

    bool submitInference = false;
    if (m_videoPlayer->isLiveCamera()) {
        submitInference = m_cameraAnalyzing;
    } else {
        // 本地文件：播放时实时推理；整段扫描进行时共用同一 worker，暂停提交播放帧
        submitInference = m_isPlaying && !m_fileBatchRunning;
    }

    if (submitInference) {
        const int stride = m_spinRealtimeInferStride->value();
        bool shouldAttempt = true;
        if (stride > 1) {
            ++m_realtimeInferFrameCounter;
            if (m_realtimeInferFrameCounter < stride)
                shouldAttempt = false;
            else
                m_realtimeInferFrameCounter = 0;
        }
        if (shouldAttempt) {
            if (!m_frameProcessor->trySubmitFrame(frame, m_videoPlayer->currentFrame())) {
                // 推理线程仍忙：下一帧立刻再尝试，不把步长空过去
                if (stride > 1)
                    m_realtimeInferFrameCounter = qMax(0, stride - 1);
            }
        }
    }

    if (!m_videoPlayer->isLiveCamera()) {
        m_slider->blockSignals(true);
        m_slider->setValue(m_videoPlayer->currentFrame());
        m_slider->blockSignals(false);
    }

    m_frameCount++;
    qint64 elapsed = m_displayTimer.elapsed();
    if (elapsed > 500) {
        double fps = 1000.0 * m_frameCount / elapsed;
        m_fpsLabel->setText(QStringLiteral("显示FPS: %1").arg(fps, 0, 'f', 1));
        m_frameCount = 0;
        m_displayTimer.restart();
    }
}

void MainWindow::onDetectionResults(const QVector<DetectionResult> &detections,
                                     const QMap<int, TrackInfo> &tracks,
                                     const QMap<PetBehavior, int> &durations,
                                     const QVector<QPair<int, double>> &timeline,
                                     int frameIndex,
                                     const QVector<DetectionResult> &personDetections)
{
    m_lastDetections = detections;
    m_lastTracks = tracks;
    m_videoWidget->setPersonDetections(personDetections);

    // 本地文件时段表仅由「整段统计」扫描写入；摄像头由实时区间写入
    if (m_videoPlayer->isLiveCamera() && m_cameraAnalyzing) {
        m_presenceRecorder.recordFrameResult(frameIndex, !detections.isEmpty());
    }

    m_pendingDurations = durations;
    m_pendingTimeline  = timeline;
    m_chartDataPending = true;

    if (m_chartUpdateTimer.elapsed() >= CHART_UPDATE_INTERVAL_MS) {
        m_trajectoryWidget->updateTrajectories(tracks, m_videoWidth, m_videoHeight);
        m_statisticsWidget->updateStatistics(m_pendingDurations, m_pendingTimeline);
        m_chartDataPending = false;
        m_chartUpdateTimer.restart();
    }
}

void MainWindow::onAlertCheck(const QMap<int, TrackInfo> &tracks)
{
    m_alertManager->check(tracks, nullptr);
}

void MainWindow::onSliderMoved(int position)
{
    if (m_videoPlayer->isLiveCamera()) return;
    m_videoPlayer->seekTo(position);
}

void MainWindow::onExportPresenceCsv()
{
    if (!m_isVideoFileMode) {
        if (m_cameraAnalyzing) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("请先点击「结束分析」结束本段采集后再导出。"));
            return;
        }
        if (!m_cameraExportReady) {
            QMessageBox::information(
                this,
                QStringLiteral("提示"),
                QStringLiteral("请先点击「开始分析」，再点「结束分析」完成本段直播采集后再导出。"));
            return;
        }
    } else {
        if (m_fileBatchRunning) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("整段统计进行中，请完成后导出。"));
            return;
        }
        if (!m_btnExport->isEnabled()) {
            QMessageBox::information(
                this,
                QStringLiteral("提示"),
                QStringLiteral("请先点击「整段统计(时段表)」生成整段时段数据后再导出。"));
            return;
        }
    }

    m_presenceRecorder.finalize();

    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出宠物出现时段"),
        QStringLiteral("宠物出现时段汇总.csv"),
        QStringLiteral("CSV 表格 (*.csv);;所有文件 (*)"));

    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");

    if (!m_presenceRecorder.exportToCsv(path)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("文件保存失败。"));
        return;
    }

    const int n = m_presenceRecorder.segmentCount();
    QMessageBox::information(
        this,
        QStringLiteral("导出成功"),
        QStringLiteral("已导出 %1 个大段连续时段。\n可用 Excel 打开该 CSV 文件。").arg(n));
}

void MainWindow::onShowHistory()
{
    auto *dlg = new HistoryDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}
