#ifndef FRAMEPROCESSOR_H
#define FRAMEPROCESSOR_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>
#include <QMap>
#include <QString>
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

    /// 尝试提交一帧供实时推理；若上一帧尚未被工作线程取走则返回 false（不拷贝，避免拖慢播放）
    bool trySubmitFrame(const cv::Mat &frame, int frameIndex);
    void requestReset();

    /// 在后台线程顺序读取整段视频并逐帧推理（与 trySubmitFrame 互斥排队执行）
    void requestFullVideoAnalysis(const QString &filePath);

    /// 整段统计加速：stride=每 N 帧做一次检测，中间帧 grab 跳过解码并沿用本次结果（推理边长固定与模型一致，见 .cpp 说明）
    void setFullVideoScanStride(int stride);

signals:
    void resultsReady(const QVector<DetectionResult> &detections,
                      const QMap<int, TrackInfo> &tracks,
                      const QMap<PetBehavior, int> &durations,
                      const QVector<QPair<int, double>> &timeline,
                      int frameIndex,
                      const QVector<DetectionResult> &personDetections);
    void alertCheck(const QMap<int, TrackInfo> &tracks);

    void fullVideoProgress(int currentFrame, int totalFrames);
    /// 整段扫描：一次覆盖 [startFrame, endFrame] 闭区间，减少跨线程信号次数
    void fullVideoStatsBlock(int startFrame, int endFrame, bool petPresent);
    void fullVideoFinished();
    void fullVideoFailed(const QString &reason);

public slots:
    void process();

private:
    void runFullVideoProcess(const QString &filePath);

    QThread         m_thread;
    QMutex          m_mutex;
    QWaitCondition  m_condition;

    cv::Mat         m_pendingFrame;
    int             m_pendingFrameIndex = 0;
    bool            m_hasPending = false;
    QString         m_fullVideoPath;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_resetRequested{false};

    YoloDetector     *m_detector = nullptr;
    PetTracker       *m_tracker  = nullptr;
    BehaviorAnalyzer *m_analyzer = nullptr;

    /// 整段扫描：推理步长（≥1），默认 3
    int m_fullScanStride = 3;
};

#endif // FRAMEPROCESSOR_H
