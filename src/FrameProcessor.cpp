#include "FrameProcessor.h"
#include "YoloDetector.h"
#include "PetTracker.h"
#include "BehaviorAnalyzer.h"
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>
#include <QtGlobal>
#include <QFile>
#include <QString>
#include <string>

namespace {

bool openVideoFileCapture(cv::VideoCapture &cap, const QString &filePath)
{
    const QByteArray enc = QFile::encodeName(filePath);
    if (!enc.isEmpty()) {
        if (cap.open(std::string(enc.constData(), static_cast<size_t>(enc.size()))))
            return true;
    }
    const QByteArray utf8 = filePath.toUtf8();
    if (cap.open(std::string(utf8.constData(), static_cast<size_t>(utf8.size()))))
        return true;
    return cap.open(filePath.toStdString());
}

} // namespace

FrameProcessor::FrameProcessor(QObject *parent)
    : QObject(parent)
{
    m_detector = new YoloDetector(nullptr);
    m_tracker  = new PetTracker(nullptr);
    m_analyzer = new BehaviorAnalyzer(nullptr);
}

FrameProcessor::~FrameProcessor()
{
    stop();
    delete m_detector;
    delete m_tracker;
    delete m_analyzer;
}

bool FrameProcessor::loadModel(const QString &onnxPath)
{
    return m_detector->loadModel(onnxPath);
}

bool FrameProcessor::isModelLoaded() const
{
    return m_detector->isLoaded();
}

void FrameProcessor::start()
{
    m_running = true;
    m_detector->moveToThread(&m_thread);
    m_tracker->moveToThread(&m_thread);
    m_analyzer->moveToThread(&m_thread);
    this->moveToThread(&m_thread);
    m_thread.start();
    QMetaObject::invokeMethod(this, "process", Qt::QueuedConnection);
}

void FrameProcessor::stop()
{
    m_running = false;
    m_condition.wakeAll();
    if (m_thread.isRunning()) {
        m_thread.quit();
        if (!m_thread.wait(3000)) {
            m_thread.terminate();
            m_thread.wait();
        }
    }
}

bool FrameProcessor::trySubmitFrame(const cv::Mat &frame, int frameIndex)
{
    QMutexLocker lock(&m_mutex);
    if (m_hasPending)
        return false;
    frame.copyTo(m_pendingFrame);
    m_pendingFrameIndex = frameIndex;
    m_hasPending = true;
    m_condition.wakeOne();
    return true;
}

void FrameProcessor::requestReset()
{
    QMutexLocker lock(&m_mutex);
    m_resetRequested = true;
    m_hasPending = false;
    m_pendingFrame.release();
    m_condition.wakeAll();
}

void FrameProcessor::requestFullVideoAnalysis(const QString &filePath)
{
    QMutexLocker lock(&m_mutex);
    m_fullVideoPath = filePath;
    m_condition.wakeAll();
}

void FrameProcessor::setFullVideoScanStride(int stride)
{
    m_fullScanStride = qBound(1, stride, 120);
}

void FrameProcessor::runFullVideoProcess(const QString &filePath)
{
    const int savedInputSize = m_detector->inputSize();
    // YOLOv8 ONNX 与 OpenCV DNN 要求输入空间尺寸与导出时一致（常见为 640）。
    // 使用 320/416 等会触发 reshape_layer 内断言失败，故整段统计必须与实时推理使用相同 inputSize。
    m_detector->setInputSize(savedInputSize);

    cv::VideoCapture cap;
    try {
        if (!openVideoFileCapture(cap, filePath) || !cap.isOpened()) {
            m_detector->setInputSize(savedInputSize);
            emit fullVideoFailed(
                QStringLiteral("无法打开视频文件（请确认路径无中文编码问题，或先关闭其它占用该文件的程序）"));
            return;
        }

        int total = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        if (total <= 0) total = 1;

        const int stride = qMax(1, m_fullScanStride);
        // 降低进度条刷新频率，减轻跨线程信号开销
        const int progressStep = qMax(5, stride * 3);

        cv::Mat frame;
        int     frameIndex = 0;

        // 整段扫描仅用于时段统计：只做检测；stride>1 时用 grab() 跳过解码，块内同一检测结果
        while (m_running) {
            if (!cap.read(frame) || frame.empty())
                break;

            frameIndex++;
            auto allDet = m_detector->detectAll(frame);
            if (!m_running)
                break;

            bool hasPet = !allDet.pets.isEmpty();
            const int blockStart = frameIndex;

            for (int k = 1; k < stride && m_running; ++k) {
                if (!cap.grab())
                    break;
                frameIndex++;
            }

            emit fullVideoStatsBlock(blockStart, frameIndex, hasPet);
            if (frameIndex % progressStep == 0 || frameIndex >= total)
                emit fullVideoProgress(frameIndex, total);
        }

        if (frameIndex > 0)
            emit fullVideoProgress(frameIndex, total);
    } catch (const cv::Exception &e) {
        m_detector->setInputSize(savedInputSize);
        cap.release();
        emit fullVideoFailed(QStringLiteral("整段统计推理异常：%1")
                                 .arg(QString::fromUtf8(e.what())));
        return;
    } catch (const std::exception &e) {
        m_detector->setInputSize(savedInputSize);
        cap.release();
        emit fullVideoFailed(QStringLiteral("整段统计异常：%1").arg(e.what()));
        return;
    } catch (...) {
        m_detector->setInputSize(savedInputSize);
        cap.release();
        emit fullVideoFailed(QStringLiteral("整段统计发生未知异常"));
        return;
    }

    m_detector->setInputSize(savedInputSize);
    cap.release();
    emit fullVideoFinished();
}

void FrameProcessor::process()
{
    cv::Mat frame;
    int     frameIndex = 0;

    while (m_running) {
        if (m_resetRequested) {
            m_tracker->reset();
            m_analyzer->reset();
            m_resetRequested = false;
        }

        QString fullPath;
        {
            QMutexLocker lock(&m_mutex);
            fullPath = m_fullVideoPath;
            m_fullVideoPath.clear();
        }
        if (!fullPath.isEmpty()) {
            runFullVideoProcess(fullPath);
            continue;
        }

        {
            QMutexLocker lock(&m_mutex);
            while (!m_hasPending && m_fullVideoPath.isEmpty() && m_running && !m_resetRequested) {
                m_condition.wait(&m_mutex, 100);
            }
            if (!m_running) break;
            if (m_resetRequested) continue;

            // 整段视频任务优先：下一轮循环开头会取走 m_fullVideoPath
            if (!m_fullVideoPath.isEmpty())
                continue;
            if (!m_hasPending)
                continue;

            std::swap(frame, m_pendingFrame);
            frameIndex = m_pendingFrameIndex;
            m_hasPending = false;
        }

        if (frame.empty()) continue;

        auto allResults = m_detector->detectAll(frame);
        if (!m_running) break;

        QVector<DetectionResult> &detections = allResults.pets;
        m_tracker->update(detections);
        m_analyzer->analyze(m_tracker->tracks());

        for (auto &det : detections) {
            const TrackInfo *t = m_tracker->getTrack(det.trackId);
            if (t) det.behavior = behaviorToString(t->currentBehavior);
        }

        emit resultsReady(detections,
                          m_tracker->tracks(),
                          m_analyzer->behaviorDurations(),
                          m_analyzer->activityTimeline(),
                          frameIndex,
                          allResults.persons);

        emit alertCheck(m_tracker->tracks());
    }
}
