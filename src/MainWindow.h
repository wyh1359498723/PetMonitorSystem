#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QMap>
#include <QPointer>
#include <opencv2/core.hpp>
#include "DetectionResult.h"
#include "PetPresenceRecorder.h"

class QProgressDialog;

class VideoPlayer;
class VideoWidget;
class FrameProcessor;
class TrajectoryWidget;
class StatisticsWidget;
class AlertManager;
class BehaviorAnalyzer;
class QDoubleSpinBox;
class QSpinBox;
class QFrame;
class QSplitter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void onOpenCamera();
    void onPlayPause();
    void onFrameReady(const cv::Mat &frame);
    void onSliderMoved(int position);
    void onExportPresenceCsv();

    void onStartCameraAnalysis();
    void onStopCameraAnalysis();

    void onFullVideoProgress(int currentFrame, int totalFrames);
    void onFullVideoStatsBlock(int startFrame, int endFrame, bool petPresent);
    void onFullVideoFinished();
    void onFullVideoFailed(const QString &reason);
    void onGenerateWholeTableStats();

    void onDetectionResults(const QVector<DetectionResult> &detections,
                            const QMap<int, TrackInfo> &tracks,
                            const QMap<PetBehavior, int> &durations,
                            const QVector<QPair<int, double>> &timeline,
                            int frameIndex);
    void onAlertCheck(const QMap<int, TrackInfo> &tracks);

private:
    void setupUi();
    void connectSignals();
    void loadModel();
    void resetPlaybackState();
    void restoreVideoPlayerAfterFullScan();
    void applyTheme();
    void animateStartup();

    VideoPlayer      *m_videoPlayer      = nullptr;
    VideoWidget      *m_videoWidget      = nullptr;
    FrameProcessor   *m_frameProcessor   = nullptr;
    TrajectoryWidget *m_trajectoryWidget = nullptr;
    StatisticsWidget *m_statisticsWidget = nullptr;
    AlertManager     *m_alertManager     = nullptr;

    QPushButton    *m_btnOpen   = nullptr;
    QPushButton    *m_btnCamera = nullptr;
    QPushButton    *m_btnPlay   = nullptr;
    QPushButton    *m_btnGenerateWholeStats = nullptr;
    QPushButton    *m_btnExport = nullptr;
    QPushButton    *m_btnStartAnalysis = nullptr;
    QPushButton    *m_btnStopAnalysis  = nullptr;
    QDoubleSpinBox *m_spinMinSegSec = nullptr;
    QSpinBox       *m_spinFullScanStride = nullptr;
    /// 实时分析：每 N 帧尝试提交一次推理（1=每帧尝试，仍可能因推理忙而跳过）
    QSpinBox       *m_spinRealtimeInferStride = nullptr;
    int             m_realtimeInferFrameCounter = 0;
    QSlider        *m_slider    = nullptr;
    QLabel         *m_statusLabel = nullptr;
    QLabel         *m_fpsLabel  = nullptr;

    QPointer<QProgressDialog> m_fullVideoProgressDlg;

    bool m_isPlaying = false;
    bool m_isVideoFileMode = false;
    QString m_currentVideoFilePath;
    /// 整段统计前保存的进度条帧号，用于统计结束后恢复预览
    int m_savedFramePosBeforeScan = 0;
    /// 本地文件：整段扫描（仅用于时段表）进行中
    bool m_fileBatchRunning = false;
    /// 摄像头：仅在「开始分析」～「结束分析」之间为 true
    bool m_cameraAnalyzing = false;
    /// 摄像头：已结束一段分析，可导出
    bool m_cameraExportReady = false;

    QVector<DetectionResult> m_lastDetections;
    QMap<int, TrackInfo>     m_lastTracks;
    int m_videoWidth  = 640;
    int m_videoHeight = 480;

    QElapsedTimer m_displayTimer;
    int m_frameCount = 0;

    QElapsedTimer m_chartUpdateTimer;
    bool m_chartDataPending = false;
    QMap<PetBehavior, int>        m_pendingDurations;
    QVector<QPair<int, double>>   m_pendingTimeline;

    PetPresenceRecorder m_presenceRecorder;

    QFrame    *m_videoFrame   = nullptr;
    QSplitter *m_mainSplitter = nullptr;
};

#endif // MAINWINDOW_H
