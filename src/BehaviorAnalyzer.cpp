#include "BehaviorAnalyzer.h"
#include <cmath>

BehaviorAnalyzer::BehaviorAnalyzer(QObject *parent)
    : QObject(parent)
{
    // Default feeding zone (can be set by user later)
    m_feedingZone = QRectF();
}

void BehaviorAnalyzer::analyze(QMap<int, TrackInfo> &tracks)
{
    m_frameCount++;
    double totalActivity = 0.0;
    int trackCount = 0;

    for (auto it = tracks.begin(); it != tracks.end(); ++it) {
        TrackInfo &track = it.value();
        PetBehavior behavior = classifyBehavior(track);
        track.currentBehavior = behavior;

        // Accumulate behavior durations
        m_behaviorDurations[behavior] = m_behaviorDurations.value(behavior, 0) + 1;

        double avgSpd = averageSpeed(track, m_behaviorWindowSize);
        totalActivity += avgSpd;
        trackCount++;
    }

    // Record activity timeline
    double avgActivity = (trackCount > 0) ? totalActivity / trackCount : 0.0;
    m_activityTimeline.append(qMakePair(m_frameCount, avgActivity));

    // Keep timeline manageable
    if (m_activityTimeline.size() > 2000) {
        m_activityTimeline.removeFirst();
    }
}

void BehaviorAnalyzer::reset()
{
    m_behaviorDurations.clear();
    m_activityTimeline.clear();
    m_frameCount = 0;
}

QMap<PetBehavior, int> BehaviorAnalyzer::behaviorDurations() const
{
    return m_behaviorDurations;
}

QVector<QPair<int, double>> BehaviorAnalyzer::activityTimeline() const
{
    return m_activityTimeline;
}

void BehaviorAnalyzer::setFeedingZone(const QRectF &zone)
{
    m_feedingZone = zone;
}

PetBehavior BehaviorAnalyzer::classifyBehavior(const TrackInfo &track) const
{
    if (track.speedHistory.isEmpty()) return PetBehavior::Unknown;

    double avgSpeed = averageSpeed(track, m_behaviorWindowSize);
    double bboxRatio = 0.0;

    if (track.lastBbox.height > 0) {
        bboxRatio = static_cast<double>(track.lastBbox.width) / track.lastBbox.height;
    }

    // Check if in feeding zone
    if (!m_feedingZone.isNull() && !track.trajectory.isEmpty()) {
        QPointF lastPos = track.trajectory.last();
        if (m_feedingZone.contains(lastPos) && avgSpeed < m_walkSpeedThreshold) {
            return PetBehavior::Eating;
        }
    }

    // Destructive / running: very fast movement
    if (avgSpeed > m_runSpeedThreshold) {
        return PetBehavior::Destructive;
    }

    // Running: fast movement
    if (avgSpeed > m_walkSpeedThreshold) {
        return PetBehavior::Running;
    }

    // Sleeping: very low movement + lying down posture (wide bbox)
    if (avgSpeed < m_sleepSpeedThreshold && bboxRatio > 1.2) {
        return PetBehavior::Sleeping;
    }

    // Sitting: low movement + upright posture
    if (avgSpeed < m_sleepSpeedThreshold && bboxRatio <= 1.2) {
        return PetBehavior::Sitting;
    }

    // Walking: moderate movement
    if (avgSpeed >= m_sleepSpeedThreshold && avgSpeed <= m_walkSpeedThreshold) {
        return PetBehavior::Walking;
    }

    return PetBehavior::Unknown;
}

double BehaviorAnalyzer::averageSpeed(const TrackInfo &track, int windowSize) const
{
    if (track.speedHistory.isEmpty()) return 0.0;

    qsizetype start = std::max(qsizetype(0), track.speedHistory.size() - qsizetype(windowSize));
    double sum = 0.0;
    int count = 0;
    for (qsizetype i = start; i < track.speedHistory.size(); ++i) {
        sum += track.speedHistory[i];
        count++;
    }
    return (count > 0) ? sum / count : 0.0;
}
