#include "VideoWidget.h"
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <opencv2/imgproc.hpp>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_labelFont("Microsoft YaHei", 10, QFont::Bold)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void VideoWidget::updateFrame(const cv::Mat &frame, const QVector<DetectionResult> &detections)
{
    if (frame.empty()) return;

    // BGR -> RGB + construct QImage, zero-copy into QImage then deep-copy once
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    m_currentImage = QImage(rgb.data, rgb.cols, rgb.rows,
                            static_cast<int>(rgb.step),
                            QImage::Format_RGB888).copy();

    m_detections = detections;
    m_needRescale = true;
    update();
}

void VideoWidget::resizeEvent(QResizeEvent * /*event*/)
{
    m_needRescale = true;
}

void VideoWidget::rebuildScaledPixmap()
{
    if (m_currentImage.isNull()) return;

    // Fast scale using nearest-neighbor — much faster than SmoothTransformation
    QImage scaled = m_currentImage.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    m_scaledPixmap = QPixmap::fromImage(std::move(scaled));

    m_offsetX = (width() - m_scaledPixmap.width()) / 2;
    m_offsetY = (height() - m_scaledPixmap.height()) / 2;

    m_scaleX = static_cast<double>(m_scaledPixmap.width()) / m_currentImage.width();
    m_scaleY = static_cast<double>(m_scaledPixmap.height()) / m_currentImage.height();

    m_needRescale = false;
}

void VideoWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);

    if (m_currentImage.isNull()) {
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("请打开视频文件"));
        return;
    }

    if (m_needRescale) {
        rebuildScaledPixmap();
    }

    // Draw black bars
    if (m_offsetX > 0) {
        painter.fillRect(0, 0, m_offsetX, height(), Qt::black);
        painter.fillRect(m_offsetX + m_scaledPixmap.width(), 0, m_offsetX + 1, height(), Qt::black);
    }
    if (m_offsetY > 0) {
        painter.fillRect(0, 0, width(), m_offsetY, Qt::black);
        painter.fillRect(0, m_offsetY + m_scaledPixmap.height(), width(), m_offsetY + 1, Qt::black);
    }

    // Draw video frame (QPixmap is GPU-accelerated on most platforms)
    painter.drawPixmap(m_offsetX, m_offsetY, m_scaledPixmap);

    if (m_detections.isEmpty()) return;

    // Draw detection overlays
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(m_labelFont);
    QFontMetrics fm(m_labelFont);

    for (const auto &det : m_detections) {
        QColor color = colorForBehavior(det.behavior);

        int x = m_offsetX + static_cast<int>(det.bbox.x * m_scaleX);
        int y = m_offsetY + static_cast<int>(det.bbox.y * m_scaleY);
        int w = static_cast<int>(det.bbox.width * m_scaleX);
        int h = static_cast<int>(det.bbox.height * m_scaleY);

        // Detection box
        painter.setPen(QPen(color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(x, y, w, h);

        // Label
        QString label = QStringLiteral("%1 %2 %3%")
                            .arg(det.className)
                            .arg(det.behavior)
                            .arg(static_cast<int>(det.confidence * 100));

        QRect textRect = fm.boundingRect(label);
        textRect.adjust(-4, -2, 4, 2);
        textRect.moveTopLeft(QPoint(x, y - textRect.height()));

        painter.fillRect(textRect, QColor(color.red(), color.green(), color.blue(), 180));
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
}

QColor VideoWidget::colorForBehavior(const QString &behavior)
{
    if (behavior == QStringLiteral("睡眠"))   return QColor(100, 149, 237);
    if (behavior == QStringLiteral("静坐"))   return QColor(60, 179, 113);
    if (behavior == QStringLiteral("走动"))   return QColor(255, 165, 0);
    if (behavior == QStringLiteral("奔跑"))   return QColor(255, 69, 0);
    if (behavior == QStringLiteral("进食"))   return QColor(50, 205, 50);
    if (behavior == QStringLiteral("拆家"))   return QColor(220, 20, 60);
    return QColor(200, 200, 200);
}
