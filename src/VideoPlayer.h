#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QElapsedTimer>
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
    /// 打开本地摄像头（deviceIndex: 通常为 0 表示默认摄像头）
    bool openCamera(int deviceIndex = 0);

    void play();
    void pause();
    void seekTo(int frameIndex);

    int totalFrames() const;
    int currentFrame() const;
    double fps() const;
    bool isPlaying() const;

    /// 是否为实时摄像头流（无总帧数、不可拖动进度）
    bool isLiveCamera() const;
    int cameraDeviceIndex() const;

    /// 释放本地文件占用（整段统计需独占打开同一文件；摄像头无效）
    void releaseCaptureForFullScan();

signals:
    void frameReady(const cv::Mat &frame);
    void playbackFinished();

private slots:
    void grabNextFrame();

private:
    void setupCaptureAfterOpen();

    cv::VideoCapture m_capture;
    QTimer           m_timer;
    QElapsedTimer    m_playbackWallClock;
    /// 本次「播放」开始时对应的帧序号（用于墙钟同步，避免与真实 FPS 漂移）
    int              m_playStartFrameIndex = 0;

    int              m_totalFrames = 0;
    int              m_currentFrame = 0;
    double           m_fps = 30.0;
    bool             m_playing = false;

    bool             m_isCamera = false;
    int              m_cameraIndex = 0;
};

#endif // VIDEOPLAYER_H
