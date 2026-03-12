#ifndef DETECTIONRESULT_H
#define DETECTIONRESULT_H

#include <QString>
#include <QPointF>
#include <QVector>
#include <opencv2/core.hpp>

struct DetectionResult {
    cv::Rect bbox;
    int classId = -1;        // COCO: 15=cat, 16=dog
    QString className;
    float confidence = 0.0f;
    QPointF center;
    int trackId = -1;
    QString behavior;
};

enum class PetBehavior {
    Unknown,
    Sleeping,
    Sitting,
    Walking,
    Running,
    Eating,
    Destructive
};

inline QString behaviorToString(PetBehavior b)
{
    switch (b) {
    case PetBehavior::Sleeping:    return QStringLiteral("睡眠");
    case PetBehavior::Sitting:     return QStringLiteral("静坐");
    case PetBehavior::Walking:     return QStringLiteral("走动");
    case PetBehavior::Running:     return QStringLiteral("奔跑");
    case PetBehavior::Eating:      return QStringLiteral("进食");
    case PetBehavior::Destructive: return QStringLiteral("拆家");
    default:                       return QStringLiteral("未知");
    }
}

struct TrackInfo {
    int trackId = -1;
    QString className;
    QVector<QPointF> trajectory;
    PetBehavior currentBehavior = PetBehavior::Unknown;
    QVector<double> speedHistory;
    cv::Rect lastBbox;
};

#endif // DETECTIONRESULT_H
