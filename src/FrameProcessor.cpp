#include "FrameProcessor.h"
#include "YoloDetector.h"
#include "PetTracker.h"
#include "BehaviorAnalyzer.h"

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

void FrameProcessor::submitFrame(const cv::Mat &frame)
{
    QMutexLocker lock(&m_mutex);
    frame.copyTo(m_pendingFrame);
    m_hasPending = true;
    m_condition.wakeOne();
}

void FrameProcessor::requestReset()
{
    QMutexLocker lock(&m_mutex);
    m_resetRequested = true;
    m_hasPending = false;
    m_pendingFrame.release();
}

void FrameProcessor::process()
{
    cv::Mat frame;

    while (m_running) {
        // Check for reset request (handled on worker thread — no race condition)
        if (m_resetRequested) {
            m_tracker->reset();
            m_analyzer->reset();
            m_resetRequested = false;
        }

        {
            QMutexLocker lock(&m_mutex);
            while (!m_hasPending && m_running && !m_resetRequested) {
                m_condition.wait(&m_mutex, 100);
            }
            if (!m_running) break;
            if (m_resetRequested) continue;

            std::swap(frame, m_pendingFrame);
            m_hasPending = false;
        }

        if (frame.empty()) continue;

        QVector<DetectionResult> detections = m_detector->detect(frame);
        if (!m_running) break;

        m_tracker->update(detections);
        m_analyzer->analyze(m_tracker->tracks());

        for (auto &det : detections) {
            const TrackInfo *t = m_tracker->getTrack(det.trackId);
            if (t) det.behavior = behaviorToString(t->currentBehavior);
        }

        emit resultsReady(detections,
                          m_tracker->tracks(),
                          m_analyzer->behaviorDurations(),
                          m_analyzer->activityTimeline());

        emit alertCheck(m_tracker->tracks());
    }
}
