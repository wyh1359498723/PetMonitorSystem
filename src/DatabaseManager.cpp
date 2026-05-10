#include "DatabaseManager.h"
#include "PetPresenceRecorder.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QVariant>

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager mgr;
    return mgr;
}

bool DatabaseManager::open(const QString &dbPath)
{
    if (m_opened) return true;

    QString path = dbPath;
    if (path.isEmpty())
        path = QCoreApplication::applicationDirPath() + "/pet_monitor.db";

    m_connectionName = QStringLiteral("PetMonitorDB");

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return false;
        }
    }

    createTables();
    m_opened = true;
    qDebug() << "Database opened:" << path;
    return true;
}

void DatabaseManager::close()
{
    if (!m_opened) return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_opened = false;
}

void DatabaseManager::createTables()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);

    q.exec("CREATE TABLE IF NOT EXISTS sessions ("
           "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "  source TEXT NOT NULL,"
           "  start_time TEXT NOT NULL,"
           "  end_time TEXT,"
           "  fps REAL DEFAULT 0,"
           "  total_frames INTEGER DEFAULT 0"
           ")");

    q.exec("CREATE TABLE IF NOT EXISTS presence_segments ("
           "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "  session_id INTEGER NOT NULL,"
           "  start_frame INTEGER,"
           "  end_frame INTEGER,"
           "  start_sec REAL,"
           "  end_sec REAL,"
           "  duration_sec REAL,"
           "  FOREIGN KEY(session_id) REFERENCES sessions(id)"
           ")");

    q.exec("CREATE TABLE IF NOT EXISTS behavior_stats ("
           "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "  session_id INTEGER NOT NULL,"
           "  behavior TEXT NOT NULL,"
           "  frame_duration INTEGER DEFAULT 0,"
           "  percentage REAL DEFAULT 0,"
           "  FOREIGN KEY(session_id) REFERENCES sessions(id)"
           ")");

    q.exec("CREATE TABLE IF NOT EXISTS alert_history ("
           "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "  session_id INTEGER NOT NULL,"
           "  timestamp TEXT NOT NULL,"
           "  message TEXT,"
           "  level TEXT,"
           "  FOREIGN KEY(session_id) REFERENCES sessions(id)"
           ")");
}

qint64 DatabaseManager::createSession(const QString &source, double fps, int totalFrames)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("INSERT INTO sessions (source, start_time, fps, total_frames) "
              "VALUES (:src, :st, :fps, :tf)");
    q.bindValue(":src", source);
    q.bindValue(":st", QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":fps", fps);
    q.bindValue(":tf", totalFrames);
    if (!q.exec()) {
        qWarning() << "createSession failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

void DatabaseManager::closeSession(qint64 sessionId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("UPDATE sessions SET end_time = :et WHERE id = :id");
    q.bindValue(":et", QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":id", sessionId);
    q.exec();
}

void DatabaseManager::savePresenceSegments(qint64 sessionId,
                                            const QVector<PetPresenceSegment> &segments)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery q(db);
    q.prepare("INSERT INTO presence_segments "
              "(session_id, start_frame, end_frame, start_sec, end_sec, duration_sec) "
              "VALUES (:sid, :sf, :ef, :ss, :es, :ds)");
    for (const auto &s : segments) {
        q.bindValue(":sid", sessionId);
        q.bindValue(":sf", s.startFrame);
        q.bindValue(":ef", s.endFrame);
        q.bindValue(":ss", s.startSec);
        q.bindValue(":es", s.endSec);
        q.bindValue(":ds", s.durationSec);
        q.exec();
    }
    db.commit();
}

void DatabaseManager::saveBehaviorStats(qint64 sessionId,
                                         const QMap<PetBehavior, int> &durations)
{
    int total = 0;
    for (auto it = durations.constBegin(); it != durations.constEnd(); ++it)
        total += it.value();
    if (total <= 0) return;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery q(db);
    q.prepare("INSERT INTO behavior_stats (session_id, behavior, frame_duration, percentage) "
              "VALUES (:sid, :beh, :fd, :pct)");
    for (auto it = durations.constBegin(); it != durations.constEnd(); ++it) {
        if (it.value() == 0) continue;
        q.bindValue(":sid", sessionId);
        q.bindValue(":beh", behaviorToString(it.key()));
        q.bindValue(":fd", it.value());
        q.bindValue(":pct", 100.0 * it.value() / total);
        q.exec();
    }
    db.commit();
}

void DatabaseManager::saveAlert(qint64 sessionId,
                                 const QString &message, const QString &level)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("INSERT INTO alert_history (session_id, timestamp, message, level) "
              "VALUES (:sid, :ts, :msg, :lv)");
    q.bindValue(":sid", sessionId);
    q.bindValue(":ts", QDateTime::currentDateTime().toString(Qt::ISODate));
    q.bindValue(":msg", message);
    q.bindValue(":lv", level);
    q.exec();
}

void DatabaseManager::deleteSession(qint64 sessionId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery q(db);

    q.prepare("DELETE FROM alert_history WHERE session_id = :sid");
    q.bindValue(":sid", sessionId);
    q.exec();

    q.prepare("DELETE FROM behavior_stats WHERE session_id = :sid");
    q.bindValue(":sid", sessionId);
    q.exec();

    q.prepare("DELETE FROM presence_segments WHERE session_id = :sid");
    q.bindValue(":sid", sessionId);
    q.exec();

    q.prepare("DELETE FROM sessions WHERE id = :sid");
    q.bindValue(":sid", sessionId);
    q.exec();

    db.commit();
}

QVector<SessionRecord> DatabaseManager::allSessions()
{
    QVector<SessionRecord> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec("SELECT id, source, start_time, end_time, fps, total_frames "
           "FROM sessions ORDER BY id DESC");
    while (q.next()) {
        SessionRecord r;
        r.id = q.value(0).toLongLong();
        r.source = q.value(1).toString();
        r.startTime = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
        r.endTime = QDateTime::fromString(q.value(3).toString(), Qt::ISODate);
        r.fps = q.value(4).toDouble();
        r.totalFrames = q.value(5).toInt();
        result.append(r);
    }
    return result;
}

QVector<PresenceRecord> DatabaseManager::presenceForSession(qint64 sessionId)
{
    QVector<PresenceRecord> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id, start_frame, end_frame, start_sec, end_sec, duration_sec "
              "FROM presence_segments WHERE session_id = :sid ORDER BY start_frame");
    q.bindValue(":sid", sessionId);
    q.exec();
    while (q.next()) {
        PresenceRecord r;
        r.id = q.value(0).toLongLong();
        r.sessionId = sessionId;
        r.startFrame = q.value(1).toInt();
        r.endFrame = q.value(2).toInt();
        r.startSec = q.value(3).toDouble();
        r.endSec = q.value(4).toDouble();
        r.durationSec = q.value(5).toDouble();
        result.append(r);
    }
    return result;
}

QVector<BehaviorRecord> DatabaseManager::behaviorForSession(qint64 sessionId)
{
    QVector<BehaviorRecord> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id, behavior, frame_duration, percentage "
              "FROM behavior_stats WHERE session_id = :sid");
    q.bindValue(":sid", sessionId);
    q.exec();
    while (q.next()) {
        BehaviorRecord r;
        r.id = q.value(0).toLongLong();
        r.sessionId = sessionId;
        r.behavior = q.value(1).toString();
        r.frameDuration = q.value(2).toInt();
        r.percentage = q.value(3).toDouble();
        result.append(r);
    }
    return result;
}

QVector<AlertHistoryRecord> DatabaseManager::alertsForSession(qint64 sessionId)
{
    QVector<AlertHistoryRecord> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare("SELECT id, timestamp, message, level "
              "FROM alert_history WHERE session_id = :sid ORDER BY id");
    q.bindValue(":sid", sessionId);
    q.exec();
    while (q.next()) {
        AlertHistoryRecord r;
        r.id = q.value(0).toLongLong();
        r.sessionId = sessionId;
        r.timestamp = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
        r.message = q.value(2).toString();
        r.level = q.value(3).toString();
        result.append(r);
    }
    return result;
}

QMap<QString, int> DatabaseManager::behaviorSummaryAllTime()
{
    QMap<QString, int> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec("SELECT behavior, SUM(frame_duration) FROM behavior_stats GROUP BY behavior");
    while (q.next()) {
        result[q.value(0).toString()] = q.value(1).toInt();
    }
    return result;
}
