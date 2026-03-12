#ifndef FRAMEPROCESSOR_H
#define FRAMEPROCESSOR_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include <QMap>
#include <atomic>
#include <opencv2/core.hpp>
#include "DetectionResult.h"

class YoloDetector;
class PetTracker;
class BehaviorAnalyzer;

class FrameProcessor : public QObject
{
    Q_OBJECT

public:
    explicit FrameProcessor(QObject *parent = nullptr);
    ~FrameProcessor();

    void start();
    void stop();

    bool loadModel(const QString &onnxPath);
    bool isModelLoaded() const;

    void submitFrame(const cv::Mat &frame);
    void requestReset();

signals:
    void resultsReady(const QVector<DetectionResult> &detections,
                      const QMap<int, TrackInfo> &tracks,
                      const QMap<PetBehavior, int> &durations,
                      const QVector<QPair<int, double>> &timeline);
    void alertCheck(const QMap<int, TrackInfo> &tracks);

public slots:
    void process();

private:
    QThread         m_thread;
    QMutex          m_mutex;
    QWaitCondition  m_condition;

    cv::Mat         m_pendingFrame;
    bool            m_hasPending = false;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_resetRequested{false};

    YoloDetector     *m_detector = nullptr;
    PetTracker       *m_tracker  = nullptr;
    BehaviorAnalyzer *m_analyzer = nullptr;
};

#endif // FRAMEPROCESSOR_H
