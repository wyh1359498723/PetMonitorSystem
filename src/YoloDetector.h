#ifndef YOLODETECTOR_H
#define YOLODETECTOR_H

#include <QObject>
#include <QVector>
#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include "DetectionResult.h"

class YoloDetector : public QObject
{
    Q_OBJECT

public:
    explicit YoloDetector(QObject *parent = nullptr);

    bool loadModel(const QString &onnxPath);
    QVector<DetectionResult> detect(const cv::Mat &frame);

    void setConfidenceThreshold(float threshold);
    void setNmsThreshold(float threshold);
    void setInputSize(int size);
    bool isLoaded() const;

private:
    struct LetterboxInfo {
        float scale;
        int padLeft;
        int padTop;
    };

    cv::Mat preprocess(const cv::Mat &frame, LetterboxInfo &info);
    QVector<DetectionResult> postprocess(const cv::Mat &output,
                                         const LetterboxInfo &info,
                                         int origWidth, int origHeight);

    cv::dnn::Net m_net;
    bool   m_loaded = false;
    int    m_inputSize = 640;
    float  m_confThreshold = 0.35f;
    float  m_nmsThreshold = 0.45f;

    // COCO class IDs we care about
    static constexpr int COCO_CAT = 15;
    static constexpr int COCO_DOG = 16;
};

#endif // YOLODETECTOR_H
