#include "PetTracker.h"
#include <algorithm>
#include <cmath>

PetTracker::PetTracker(QObject *parent)
    : QObject(parent)
{
}

void PetTracker::update(QVector<DetectionResult> &detections)
{
    // Hungarian-like greedy IoU matching
    QVector<bool> matched(detections.size(), false);
    QMap<int, bool> trackMatched;

    for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it) {
        trackMatched[it.key()] = false;
    }

    // For each existing track, find best matching detection by IoU
    for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it) {
        int trackId = it.key();
        TrackInfo &track = it.value();

        float bestIoU = 0.3f; // minimum IoU threshold for matching
        int bestIdx = -1;

        for (int i = 0; i < detections.size(); ++i) {
            if (matched[i]) continue;
            float iou = computeIoU(track.lastBbox, detections[i].bbox);
            if (iou > bestIoU) {
                bestIoU = iou;
                bestIdx = i;
            }
        }

        if (bestIdx >= 0) {
            // Update track with new detection
            matched[bestIdx] = true;
            trackMatched[trackId] = true;

            detections[bestIdx].trackId = trackId;
            track.lastBbox = detections[bestIdx].bbox;
            track.trajectory.append(detections[bestIdx].center);

            // Compute speed from last two trajectory points
            if (track.trajectory.size() >= 2) {
                QPointF prev = track.trajectory[track.trajectory.size() - 2];
                QPointF curr = track.trajectory.last();
                double dx = curr.x() - prev.x();
                double dy = curr.y() - prev.y();
                double speed = std::sqrt(dx * dx + dy * dy);
                track.speedHistory.append(speed);
            }

            // Limit trajectory length
            if (track.trajectory.size() > m_maxTrajectoryLength) {
                track.trajectory.removeFirst();
            }
            if (track.speedHistory.size() > m_maxTrajectoryLength) {
                track.speedHistory.removeFirst();
            }

            m_missedFrames[trackId] = 0;
        }
    }

    // Increment missed frames for unmatched tracks and remove stale ones
    QList<int> toRemove;
    for (auto it = trackMatched.begin(); it != trackMatched.end(); ++it) {
        if (!it.value()) {
            m_missedFrames[it.key()] = m_missedFrames.value(it.key(), 0) + 1;
            if (m_missedFrames[it.key()] > m_maxMissedFrames) {
                toRemove.append(it.key());
            }
        }
    }
    for (int id : toRemove) {
        m_tracks.remove(id);
        m_missedFrames.remove(id);
    }

    // Create new tracks for unmatched detections
    for (int i = 0; i < detections.size(); ++i) {
        if (matched[i]) continue;

        int newId = m_nextTrackId++;
        detections[i].trackId = newId;

        TrackInfo newTrack;
        newTrack.trackId = newId;
        newTrack.className = detections[i].className;
        newTrack.lastBbox = detections[i].bbox;
        newTrack.trajectory.append(detections[i].center);

        m_tracks.insert(newId, newTrack);
        m_missedFrames[newId] = 0;
    }
}

void PetTracker::reset()
{
    m_tracks.clear();
    m_missedFrames.clear();
    m_nextTrackId = 0;
}

const QMap<int, TrackInfo> &PetTracker::tracks() const
{
    return m_tracks;
}

QMap<int, TrackInfo> &PetTracker::tracks()
{
    return m_tracks;
}

const TrackInfo *PetTracker::getTrack(int trackId) const
{
    auto it = m_tracks.constFind(trackId);
    if (it != m_tracks.constEnd())
        return &it.value();
    return nullptr;
}

void PetTracker::setMaxTrajectoryLength(int len)
{
    m_maxTrajectoryLength = len;
}

float PetTracker::computeIoU(const cv::Rect &a, const cv::Rect &b) const
{
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    if (x2 <= x1 || y2 <= y1) return 0.0f;

    float intersection = static_cast<float>((x2 - x1) * (y2 - y1));
    float areaA = static_cast<float>(a.width * a.height);
    float areaB = static_cast<float>(b.width * b.height);
    float unionArea = areaA + areaB - intersection;

    return (unionArea > 0) ? intersection / unionArea : 0.0f;
}
