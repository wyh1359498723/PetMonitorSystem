#include "MainWindow.h"
#include "VideoPlayer.h"
#include "VideoWidget.h"
#include "FrameProcessor.h"
#include "TrajectoryWidget.h"
#include "StatisticsWidget.h"
#include "AlertManager.h"
#include "BehaviorAnalyzer.h"
#include "DetectionResult.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QStatusBar>
#include <QGroupBox>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <opencv2/core.hpp>

static constexpr int CHART_UPDATE_INTERVAL_MS = 500;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("宠物居家场景可视化系统"));
    resize(1400, 850);

    m_videoPlayer      = new VideoPlayer(this);
    m_videoWidget      = new VideoWidget(this);
    m_frameProcessor   = new FrameProcessor(nullptr);
    m_trajectoryWidget = new TrajectoryWidget(this);
    m_statisticsWidget = new StatisticsWidget(this);
    m_alertManager     = new AlertManager(this);

    setupUi();
    connectSignals();
    loadModel();

    m_displayTimer.start();
    m_chartUpdateTimer.start();
}

MainWindow::~MainWindow()
{
    m_frameProcessor->stop();
    delete m_frameProcessor;
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
                statusBar()->showMessage(QStringLiteral("模型加载成功，请打开视频文件"));
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

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    m_videoWidget->setMinimumSize(640, 480);

    auto *controlGroup = new QGroupBox(QStringLiteral("控制面板"));
    auto *controlLayout = new QVBoxLayout(controlGroup);

    auto *btnLayout = new QHBoxLayout;
    m_btnOpen = new QPushButton(QStringLiteral("打开视频"));
    m_btnPlay = new QPushButton(QStringLiteral("播放"));
    m_btnPlay->setEnabled(false);
    btnLayout->addWidget(m_btnOpen);
    btnLayout->addWidget(m_btnPlay);
    controlLayout->addLayout(btnLayout);

    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setEnabled(false);
    controlLayout->addWidget(m_slider);

    m_fpsLabel = new QLabel(QStringLiteral("FPS: --"));
    controlLayout->addWidget(m_fpsLabel);

    auto *trajGroup = new QGroupBox(QStringLiteral("运动轨迹"));
    auto *trajLayout = new QVBoxLayout(trajGroup);
    m_trajectoryWidget->setMinimumHeight(200);
    trajLayout->addWidget(m_trajectoryWidget);

    auto *statsGroup = new QGroupBox(QStringLiteral("行为统计"));
    auto *statsLayout = new QVBoxLayout(statsGroup);
    m_statisticsWidget->setMinimumHeight(250);
    statsLayout->addWidget(m_statisticsWidget);

    auto *rightPanel = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->addWidget(controlGroup);
    rightLayout->addWidget(trajGroup, 1);
    rightLayout->addWidget(statsGroup, 1);
    rightPanel->setFixedWidth(420);

    m_alertManager->setFixedHeight(0);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(m_alertManager);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_videoWidget);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);

    m_statusLabel = new QLabel(QStringLiteral("就绪"));
    statusBar()->addPermanentWidget(m_statusLabel);
}

void MainWindow::connectSignals()
{
    connect(m_btnOpen, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(m_btnPlay, &QPushButton::clicked, this, &MainWindow::onPlayPause);
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
}

void MainWindow::onOpenFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择视频文件"), QString(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov *.flv);;所有文件 (*)")
    );
    if (filePath.isEmpty()) return;

    // Pause playback first, then reset processor
    if (m_isPlaying) {
        m_videoPlayer->pause();
        m_isPlaying = false;
    }

    if (!m_videoPlayer->openFile(filePath)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法打开视频文件：%1").arg(filePath));
        return;
    }

    m_slider->setRange(0, m_videoPlayer->totalFrames() - 1);
    m_slider->setEnabled(true);
    m_btnPlay->setEnabled(true);
    m_btnPlay->setText(QStringLiteral("播放"));

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

    m_statusLabel->setText(QStringLiteral("已加载：%1").arg(filePath));
}

void MainWindow::onPlayPause()
{
    if (m_isPlaying) {
        m_videoPlayer->pause();
        m_btnPlay->setText(QStringLiteral("播放"));
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
    m_frameProcessor->submitFrame(frame);

    m_slider->blockSignals(true);
    m_slider->setValue(m_videoPlayer->currentFrame());
    m_slider->blockSignals(false);

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
                                     const QVector<QPair<int, double>> &timeline)
{
    m_lastDetections = detections;
    m_lastTracks = tracks;

    // Always store latest data, but throttle expensive chart redraws
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
    m_videoPlayer->seekTo(position);
}
