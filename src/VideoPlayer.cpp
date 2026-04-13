#include "VideoPlayer.h"
#include <QDebug>
#include <QFile>
#include <QThread>
#include <string>

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &VideoPlayer::grabNextFrame);
}

VideoPlayer::~VideoPlayer()
{
    m_timer.stop();
    if (m_capture.isOpened())
        m_capture.release();
}

void VideoPlayer::setupCaptureAfterOpen()
{
    if (m_isCamera) {
        m_totalFrames = 0;
        m_currentFrame = 0;
        m_fps = m_capture.get(cv::CAP_PROP_FPS);
        if (m_fps <= 0 || m_fps > 120.0) m_fps = 30.0;
        // 固定常用分辨率，提高兼容性（驱动可能忽略但多数有效）
        m_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    } else {
        m_totalFrames = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
        if (m_totalFrames < 0) m_totalFrames = 0;
        m_currentFrame = 0;

        m_fps = m_capture.get(cv::CAP_PROP_FPS);
        // 部分封装在首帧解码后 FPS 才准确
        cv::Mat probe;
        if (m_capture.read(probe) && !probe.empty()) {
            const double f2 = m_capture.get(cv::CAP_PROP_FPS);
            if (f2 > 1e-3 && f2 <= 120.0) m_fps = f2;
        }
        m_capture.set(cv::CAP_PROP_POS_FRAMES, 0);
        m_currentFrame = 0;

        if (m_fps <= 1e-3 || m_fps > 120.0) {
            // 常见片源回退（仍可能不准，但优于 30 硬套）
            m_fps = 25.0;
        }
    }
}

static bool openCameraBackend(cv::VideoCapture &cap, int deviceIndex, int api)
{
    cap.release();
    return cap.open(deviceIndex, api);
}

bool VideoPlayer::openFile(const QString &filePath)
{
    m_timer.stop();
    m_playing = false;
    m_isCamera = false;

    if (m_capture.isOpened())
        m_capture.release();

    // 使用本地编码路径，避免中文路径下 toStdString() 在 MSVC 上打开失败
    const QByteArray enc = QFile::encodeName(filePath);
    bool opened = false;
    if (!enc.isEmpty()) {
        opened = m_capture.open(
            std::string(enc.constData(), static_cast<size_t>(enc.size())));
    }
    if (!opened) {
        const QByteArray utf8 = filePath.toUtf8();
        opened = m_capture.open(
            std::string(utf8.constData(), static_cast<size_t>(utf8.size())));
    }
    if (!opened) {
        qWarning() << "Failed to open video:" << filePath;
        return false;
    }

    setupCaptureAfterOpen();
    return true;
}

void VideoPlayer::releaseCaptureForFullScan()
{
    if (m_isCamera)
        return;
    m_timer.stop();
    m_playing = false;
    if (m_capture.isOpened())
        m_capture.release();
}

bool VideoPlayer::openCamera(int deviceIndex)
{
    m_timer.stop();
    m_playing = false;
    m_isCamera = true;
    m_cameraIndex = deviceIndex;

    if (m_capture.isOpened())
        m_capture.release();

    bool ok = false;
#ifdef _WIN32
    // 多数 Win10/11 下 MSMF 对 USB 摄像头更稳定；DSHOW 易前几帧为空
    if (openCameraBackend(m_capture, deviceIndex, cv::CAP_MSMF))
        ok = true;
    else if (openCameraBackend(m_capture, deviceIndex, cv::CAP_DSHOW))
        ok = true;
    else
        ok = m_capture.open(deviceIndex);
#else
    ok = m_capture.open(deviceIndex);
#endif

    if (!ok) {
        qWarning() << "Failed to open camera device:" << deviceIndex;
        m_isCamera = false;
        return false;
    }

    setupCaptureAfterOpen();

    // 预热：丢弃若干帧，直到读到非空画面（DSHOW/部分驱动常见问题）
    cv::Mat warm;
    for (int i = 0; i < 60; ++i) {
        if (m_capture.read(warm) && !warm.empty())
            break;
        QThread::msleep(10);
    }
    if (!warm.empty()) {
        qDebug() << "Camera warmup ok, frame:" << warm.cols << "x" << warm.rows
                 << "channels:" << warm.channels();
    } else {
        qWarning() << "Camera opened but warmup could not read a frame";
    }

    return true;
}

void VideoPlayer::play()
{
    if (!m_capture.isOpened()) return;
    m_playing = true;
    m_playbackWallClock.restart();
    // 下一帧解码前的「逻辑帧序号」，与 grabNextFrame 内 POS_FRAMES 对齐
    m_playStartFrameIndex = m_currentFrame;
    m_timer.start(0);
}

void VideoPlayer::pause()
{
    m_playing = false;
    m_timer.stop();
}

void VideoPlayer::seekTo(int frameIndex)
{
    if (!m_capture.isOpened() || m_isCamera) return;

    if (m_totalFrames <= 0) return;
    frameIndex = qBound(0, frameIndex, m_totalFrames - 1);
    m_capture.set(cv::CAP_PROP_POS_FRAMES, frameIndex);
    m_currentFrame = frameIndex;

    cv::Mat frame;
    if (m_capture.read(frame)) {
        m_currentFrame = frameIndex + 1;
        emit frameReady(frame);
    }
}

int VideoPlayer::totalFrames() const { return m_totalFrames; }
int VideoPlayer::currentFrame() const { return m_currentFrame; }
double VideoPlayer::fps() const { return m_fps; }
bool VideoPlayer::isPlaying() const { return m_playing; }
bool VideoPlayer::isLiveCamera() const { return m_isCamera; }
int VideoPlayer::cameraDeviceIndex() const { return m_cameraIndex; }

void VideoPlayer::grabNextFrame()
{
    if (!m_capture.isOpened() || !m_playing) return;

    cv::Mat frame;
    // 摄像头偶发空帧：多读几次再放弃本周期
    int attempts = m_isCamera ? 5 : 1;
    for (int a = 0; a < attempts; ++a) {
        if (m_capture.read(frame) && !frame.empty())
            break;
        frame.release();
    }

    if (frame.empty()) {
        if (!m_isCamera) {
            m_timer.stop();
            m_playing = false;
            emit playbackFinished();
        } else if (m_playing) {
            // 摄像头偶发空帧：稍后重试，避免单帧失败导致整条播放链停止
            m_timer.start(15);
        }
        return;
    }

    if (m_isCamera) {
        ++m_currentFrame;
    } else {
        m_currentFrame = static_cast<int>(m_capture.get(cv::CAP_PROP_POS_FRAMES));
    }

    emit frameReady(frame);

    if (!m_playing) return;

    // 墙钟同步：按「已播放帧数 × 帧周期」对齐真实时间，减少主线程略慢导致的累计漂移
    const double framesSinceStart =
        static_cast<double>(m_currentFrame - m_playStartFrameIndex);
    const double expectedMs = framesSinceStart * 1000.0 / m_fps;
    const qint64 elapsed = m_playbackWallClock.elapsed();
    int delay = static_cast<int>(qRound(expectedMs - static_cast<double>(elapsed)));
    if (delay < 1) delay = 1;
    m_timer.start(delay);
}
