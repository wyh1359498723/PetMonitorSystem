#ifndef TRAJECTORYWIDGET_H
#define TRAJECTORYWIDGET_H

#include <QWidget>
#include <QMap>
#include "DetectionResult.h"

class TrajectoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrajectoryWidget(QWidget *parent = nullptr);

    void updateTrajectories(const QMap<int, TrackInfo> &tracks,
                            int videoWidth, int videoHeight);
    void clearTrajectories();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor trackColor(int trackId) const;

    QMap<int, TrackInfo> m_tracks;
    int m_videoWidth = 640;
    int m_videoHeight = 480;
};

#endif // TRAJECTORYWIDGET_H
