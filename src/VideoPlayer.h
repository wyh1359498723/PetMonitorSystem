#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QTimer>
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>

class VideoPlayer : public QObject
{
    Q_OBJECT

public:
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    bool openFile(const QString &filePath);
    void play();
    void pause();
    void seekTo(int frameIndex);

    int totalFrames() const;
    int currentFrame() const;
    double fps() const;
    bool isPlaying() const;

signals:
    void frameReady(const cv::Mat &frame);
    void playbackFinished();

private slots:
    void grabNextFrame();

private:
    cv::VideoCapture m_capture;
    QTimer           m_timer;
    int              m_totalFrames = 0;
    int              m_currentFrame = 0;
    double           m_fps = 30.0;
    bool             m_playing = false;
};

#endif // VIDEOPLAYER_H
