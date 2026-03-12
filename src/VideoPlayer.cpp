#include "VideoPlayer.h"
#include <QDebug>

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent)
{
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &VideoPlayer::grabNextFrame);
}

VideoPlayer::~VideoPlayer()
{
    m_timer.stop();
    if (m_capture.isOpened())
        m_capture.release();
}

bool VideoPlayer::openFile(const QString &filePath)
{
    m_timer.stop();
    m_playing = false;

    if (m_capture.isOpened())
        m_capture.release();

    if (!m_capture.open(filePath.toStdString())) {
        qWarning() << "Failed to open video:" << filePath;
        return false;
    }

    m_totalFrames = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
    m_fps = m_capture.get(cv::CAP_PROP_FPS);
    if (m_fps <= 0) m_fps = 30.0;
    m_currentFrame = 0;

    return true;
}

void VideoPlayer::play()
{
    if (!m_capture.isOpened()) return;
    m_playing = true;
    int interval = static_cast<int>(1000.0 / m_fps);
    if (interval < 1) interval = 1;
    m_timer.start(interval);
}

void VideoPlayer::pause()
{
    m_playing = false;
    m_timer.stop();
}

void VideoPlayer::seekTo(int frameIndex)
{
    if (!m_capture.isOpened()) return;
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

void VideoPlayer::grabNextFrame()
{
    if (!m_capture.isOpened()) return;

    cv::Mat frame;
    if (!m_capture.read(frame)) {
        m_timer.stop();
        m_playing = false;
        emit playbackFinished();
        return;
    }

    m_currentFrame = static_cast<int>(m_capture.get(cv::CAP_PROP_POS_FRAMES));
    emit frameReady(frame);
}
