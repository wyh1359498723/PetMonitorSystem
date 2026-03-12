#include "TrajectoryWidget.h"
#include <QPainter>
#include <QPen>
#include <QBrush>

static const QColor kPalette[] = {
    QColor(231, 76, 60),
    QColor(52, 152, 219),
    QColor(46, 204, 113),
    QColor(241, 196, 15),
    QColor(155, 89, 182),
    QColor(230, 126, 34),
    QColor(26, 188, 156),
    QColor(192, 57, 43),
};

TrajectoryWidget::TrajectoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(40, 40, 50));
    setPalette(pal);
}

void TrajectoryWidget::updateTrajectories(const QMap<int, TrackInfo> &tracks,
                                           int videoWidth, int videoHeight)
{
    m_tracks = tracks;
    m_videoWidth = videoWidth;
    m_videoHeight = videoHeight;
    update();
}

void TrajectoryWidget::clearTrajectories()
{
    m_tracks.clear();
    update();
}

void TrajectoryWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    if (m_tracks.isEmpty()) {
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无轨迹数据"));
        return;
    }

    // Draw a grid background
    painter.setPen(QPen(QColor(70, 70, 80), 1, Qt::DotLine));
    for (int x = 0; x < w; x += 40) painter.drawLine(x, 0, x, h);
    for (int y = 0; y < h; y += 40) painter.drawLine(0, y, w, y);

    // Scale factors from video coords to widget coords
    double scaleX = static_cast<double>(w) / m_videoWidth;
    double scaleY = static_cast<double>(h) / m_videoHeight;

    for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
        const TrackInfo &track = it.value();
        if (track.trajectory.size() < 2) continue;

        QColor color = trackColor(track.trackId);

        // Draw trajectory line
        QPen linePen(color, 2);
        painter.setPen(linePen);

        for (int i = 1; i < track.trajectory.size(); ++i) {
            // Fade older segments
            int alpha = 80 + (175 * i / track.trajectory.size());
            QColor segColor = color;
            segColor.setAlpha(alpha);
            painter.setPen(QPen(segColor, 2));

            QPointF p1(track.trajectory[i - 1].x() * scaleX,
                       track.trajectory[i - 1].y() * scaleY);
            QPointF p2(track.trajectory[i].x() * scaleX,
                       track.trajectory[i].y() * scaleY);
            painter.drawLine(p1, p2);
        }

        // Draw current position as a filled circle
        QPointF lastPt(track.trajectory.last().x() * scaleX,
                       track.trajectory.last().y() * scaleY);
        painter.setBrush(QBrush(color));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(lastPt, 5, 5);

        // Label
        painter.setPen(Qt::white);
        QFont font("Microsoft YaHei", 8);
        painter.setFont(font);
        QString label = QStringLiteral("ID:%1 %2").arg(track.trackId).arg(track.className);
        painter.drawText(lastPt + QPointF(8, -4), label);
    }
}

QColor TrajectoryWidget::trackColor(int trackId) const
{
    int idx = trackId % (sizeof(kPalette) / sizeof(kPalette[0]));
    return kPalette[idx];
}
