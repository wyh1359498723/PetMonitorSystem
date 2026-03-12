#ifndef PETTRACKER_H
#define PETTRACKER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include "DetectionResult.h"

class PetTracker : public QObject
{
    Q_OBJECT

public:
    explicit PetTracker(QObject *parent = nullptr);

    void update(QVector<DetectionResult> &detections);
    void reset();

    const QMap<int, TrackInfo> &tracks() const;
    QMap<int, TrackInfo> &tracks();
    const TrackInfo *getTrack(int trackId) const;

    void setMaxTrajectoryLength(int len);

private:
    float computeIoU(const cv::Rect &a, const cv::Rect &b) const;

    QMap<int, TrackInfo> m_tracks;
    int m_nextTrackId = 0;
    int m_maxTrajectoryLength = 500;
    int m_maxMissedFrames = 15;

    // Track missed frame count
    QMap<int, int> m_missedFrames;
};

#endif // PETTRACKER_H
