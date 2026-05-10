#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include "DetectionResult.h"

struct SessionRecord {
    qint64   id = 0;
    QString  source;        // 文件路径 or "camera:N"
    QDateTime startTime;
    QDateTime endTime;
    double   fps = 0.0;
    int      totalFrames = 0;
};

struct PresenceRecord {
    qint64 id = 0;
    qint64 sessionId = 0;
    int    startFrame = 0;
    int    endFrame   = 0;
    double startSec   = 0.0;
    double endSec     = 0.0;
    double durationSec = 0.0;
};

struct BehaviorRecord {
    qint64 id = 0;
    qint64 sessionId = 0;
    QString behavior;
    int     frameDuration = 0;
    double  percentage = 0.0;
};

struct AlertHistoryRecord {
    qint64  id = 0;
    qint64  sessionId = 0;
    QDateTime timestamp;
    QString message;
    QString level;
};

class DatabaseManager
{
public:
    static DatabaseManager &instance();

    bool open(const QString &dbPath = QString());
    void close();

    qint64 createSession(const QString &source, double fps, int totalFrames);
    void   closeSession(qint64 sessionId);

    void savePresenceSegments(qint64 sessionId,
                              const QVector<struct PetPresenceSegment> &segments);

    void saveBehaviorStats(qint64 sessionId,
                           const QMap<PetBehavior, int> &durations);

    void saveAlert(qint64 sessionId,
                   const QString &message, const QString &level);

    QVector<SessionRecord>      allSessions();
    QVector<PresenceRecord>     presenceForSession(qint64 sessionId);
    QVector<BehaviorRecord>     behaviorForSession(qint64 sessionId);
    QVector<AlertHistoryRecord> alertsForSession(qint64 sessionId);

    void deleteSession(qint64 sessionId);

    QMap<QString, int> behaviorSummaryAllTime();

private:
    DatabaseManager() = default;
    void createTables();

    QString m_connectionName;
    bool    m_opened = false;
};

#endif // DATABASEMANAGER_H
