#ifndef BEHAVIORANALYZER_H
#define BEHAVIORANALYZER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QRectF>
#include "DetectionResult.h"

class BehaviorAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit BehaviorAnalyzer(QObject *parent = nullptr);

    void analyze(QMap<int, TrackInfo> &tracks);
    void reset();

    // Duration in frame counts for each behavior type
    QMap<PetBehavior, int> behaviorDurations() const;

    // Timeline: list of (frameIndex, activityLevel) for line chart
    QVector<QPair<int, double>> activityTimeline() const;

    void setFeedingZone(const QRectF &zone);

private:
    PetBehavior classifyBehavior(const TrackInfo &track) const;
    double averageSpeed(const TrackInfo &track, int windowSize) const;

    QMap<PetBehavior, int> m_behaviorDurations;
    QVector<QPair<int, double>> m_activityTimeline;
    int m_frameCount = 0;

    QRectF m_feedingZone;

    // Thresholds (in pixels per frame)
    double m_sleepSpeedThreshold = 2.0;
    double m_walkSpeedThreshold = 15.0;
    double m_runSpeedThreshold = 30.0;
    int    m_behaviorWindowSize = 5;
};

#endif // BEHAVIORANALYZER_H
