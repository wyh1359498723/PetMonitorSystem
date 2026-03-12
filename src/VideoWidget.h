#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QVector>
#include <opencv2/core.hpp>
#include "DetectionResult.h"

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);

    void updateFrame(const cv::Mat &frame, const QVector<DetectionResult> &detections);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildScaledPixmap();
    QColor colorForBehavior(const QString &behavior);

    QImage  m_currentImage;
    QPixmap m_scaledPixmap;
    QVector<DetectionResult> m_detections;

    int m_offsetX = 0;
    int m_offsetY = 0;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    bool m_needRescale = true;

    QFont m_labelFont;
};

#endif // VIDEOWIDGET_H
