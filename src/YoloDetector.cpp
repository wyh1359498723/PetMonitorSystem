#include "YoloDetector.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

YoloDetector::YoloDetector(QObject *parent)
    : QObject(parent)
{
}

bool YoloDetector::loadModel(const QString &onnxPath)
{
    QFileInfo fi(onnxPath);
    if (!fi.exists()) {
        qWarning() << "Model file not found:" << fi.absoluteFilePath();
        m_loaded = false;
        return false;
    }

    QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
    qDebug() << "Loading YOLO model from:" << nativePath;

    try {
        std::string pathStr = nativePath.toLocal8Bit().constData();
        m_net = cv::dnn::readNetFromONNX(pathStr);
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        m_loaded = true;
        qDebug() << "YOLO model loaded successfully";
    } catch (const cv::Exception &e) {
        qWarning() << "Failed to load YOLO model:" << e.what();
        m_loaded = false;
    }
    return m_loaded;
}

QVector<DetectionResult> YoloDetector::detect(const cv::Mat &frame)
{
    QVector<DetectionResult> results;
    if (!m_loaded || frame.empty()) return results;

    LetterboxInfo info;
    cv::Mat blob = preprocess(frame, info);

    m_net.setInput(blob);
    cv::Mat output = m_net.forward();

    results = postprocess(output, info, frame.cols, frame.rows);
    return results;
}

cv::Mat YoloDetector::preprocess(const cv::Mat &frame, LetterboxInfo &info)
{
    int imgW = frame.cols;
    int imgH = frame.rows;

    // Letterbox resize: scale to fit m_inputSize while keeping aspect ratio
    float scale = std::min(static_cast<float>(m_inputSize) / imgW,
                           static_cast<float>(m_inputSize) / imgH);
    int newW = static_cast<int>(imgW * scale);
    int newH = static_cast<int>(imgH * scale);

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(newW, newH));

    // Pad to m_inputSize x m_inputSize with gray (114, 114, 114)
    cv::Mat padded(m_inputSize, m_inputSize, CV_8UC3, cv::Scalar(114, 114, 114));
    int padLeft = (m_inputSize - newW) / 2;
    int padTop = (m_inputSize - newH) / 2;
    resized.copyTo(padded(cv::Rect(padLeft, padTop, newW, newH)));

    info.scale = scale;
    info.padLeft = padLeft;
    info.padTop = padTop;

    // Convert to blob: BGR->RGB, normalize to [0,1], NCHW layout
    cv::Mat blob;
    cv::dnn::blobFromImage(padded, blob, 1.0 / 255.0, cv::Size(), cv::Scalar(), true, false);
    return blob;
}

QVector<DetectionResult> YoloDetector::postprocess(const cv::Mat &output,
                                                    const LetterboxInfo &info,
                                                    int origWidth, int origHeight)
{
    /*
     * YOLOv8 output shape: [1, 84, 8400] (for 80 COCO classes)
     * Rows 0-3: cx, cy, w, h
     * Rows 4-83: class scores
     * We need to transpose to [8400, 84] for easier processing.
     */
    QVector<DetectionResult> results;

    // output is [1, 84, 8400], reshape to [84, 8400]
    cv::Mat det = output.reshape(1, output.size[1]); // [84, 8400]
    cv::transpose(det, det); // [8400, 84]

    int numDetections = det.rows;
    int numClasses = det.cols - 4;

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (int i = 0; i < numDetections; ++i) {
        const float *row = det.ptr<float>(i);

        // Find best class score (columns 4..83)
        float maxScore = 0.0f;
        int maxClassId = -1;
        for (int c = 4; c < det.cols; ++c) {
            if (row[c] > maxScore) {
                maxScore = row[c];
                maxClassId = c - 4;
            }
        }

        if (maxScore < m_confThreshold) continue;

        // Keep person (0), cat (15), dog (16)
        if (maxClassId != COCO_PERSON && maxClassId != COCO_CAT && maxClassId != COCO_DOG)
            continue;

        // cx, cy, w, h in letterboxed coordinate space
        float cx = row[0];
        float cy = row[1];
        float bw = row[2];
        float bh = row[3];

        // Convert from letterbox coords to original image coords
        float x1 = (cx - bw / 2.0f - info.padLeft) / info.scale;
        float y1 = (cy - bh / 2.0f - info.padTop) / info.scale;
        float x2 = (cx + bw / 2.0f - info.padLeft) / info.scale;
        float y2 = (cy + bh / 2.0f - info.padTop) / info.scale;

        // Clamp to image bounds
        x1 = std::max(0.0f, std::min(x1, static_cast<float>(origWidth)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(origHeight)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(origWidth)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(origHeight)));

        int finalW = static_cast<int>(x2 - x1);
        int finalH = static_cast<int>(y2 - y1);
        if (finalW <= 0 || finalH <= 0) continue;

        boxes.emplace_back(static_cast<int>(x1), static_cast<int>(y1), finalW, finalH);
        confidences.push_back(maxScore);
        classIds.push_back(maxClassId);
    }

    // NMS
    std::vector<int> nmsIndices;
    cv::dnn::NMSBoxes(boxes, confidences, m_confThreshold, m_nmsThreshold, nmsIndices);

    for (int idx : nmsIndices) {
        DetectionResult r;
        r.bbox = boxes[idx];
        r.classId = classIds[idx];
        r.confidence = confidences[idx];
        r.center = QPointF(boxes[idx].x + boxes[idx].width / 2.0,
                           boxes[idx].y + boxes[idx].height / 2.0);
        if (classIds[idx] == COCO_PERSON) {
            r.className = "person";
            r.isPerson = true;
        } else {
            r.className = (classIds[idx] == COCO_CAT) ? "cat" : "dog";
            r.isPerson = false;
        }
        results.append(r);
    }

    return results;
}

YoloDetector::DetectAllResult YoloDetector::detectAll(const cv::Mat &frame)
{
    DetectAllResult result;
    QVector<DetectionResult> all = detect(frame);
    for (auto &d : all) {
        if (d.isPerson)
            result.persons.append(d);
        else
            result.pets.append(d);
    }
    return result;
}

void YoloDetector::setConfidenceThreshold(float threshold) { m_confThreshold = threshold; }
void YoloDetector::setNmsThreshold(float threshold) { m_nmsThreshold = threshold; }
void YoloDetector::setInputSize(int size) { m_inputSize = size; }
int YoloDetector::inputSize() const { return m_inputSize; }
bool YoloDetector::isLoaded() const { return m_loaded; }
