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
#include <opencv2/core.hpp>
#include "DetectionResult.h"

class VideoPlayer;
class VideoWidget;
class FrameProcessor;
class TrajectoryWidget;
class StatisticsWidget;
class AlertManager;
class BehaviorAnalyzer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onOpenFile();
    void onPlayPause();
    void onFrameReady(const cv::Mat &frame);
    void onSliderMoved(int position);

    void onDetectionResults(const QVector<DetectionResult> &detections,
                            const QMap<int, TrackInfo> &tracks,
                            const QMap<PetBehavior, int> &durations,
                            const QVector<QPair<int, double>> &timeline);
    void onAlertCheck(const QMap<int, TrackInfo> &tracks);

private:
    void setupUi();
    void connectSignals();
    void loadModel();

    VideoPlayer      *m_videoPlayer      = nullptr;
    VideoWidget      *m_videoWidget      = nullptr;
    FrameProcessor   *m_frameProcessor   = nullptr;
    TrajectoryWidget *m_trajectoryWidget = nullptr;
    StatisticsWidget *m_statisticsWidget = nullptr;
    AlertManager     *m_alertManager     = nullptr;

    QPushButton *m_btnOpen   = nullptr;
    QPushButton *m_btnPlay   = nullptr;
    QSlider     *m_slider    = nullptr;
    QLabel      *m_statusLabel = nullptr;
    QLabel      *m_fpsLabel  = nullptr;

    bool m_isPlaying = false;

    QVector<DetectionResult> m_lastDetections;
    QMap<int, TrackInfo>     m_lastTracks;
    int m_videoWidth  = 640;
    int m_videoHeight = 480;

    QElapsedTimer m_displayTimer;
    int m_frameCount = 0;

    // Throttle: pending data for deferred chart/trajectory update
    QElapsedTimer m_chartUpdateTimer;
    bool m_chartDataPending = false;
    QMap<PetBehavior, int>        m_pendingDurations;
    QVector<QPair<int, double>>   m_pendingTimeline;
};

#endif // MAINWINDOW_H
