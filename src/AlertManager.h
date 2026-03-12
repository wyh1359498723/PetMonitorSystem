#ifndef ALERTMANAGER_H
#define ALERTMANAGER_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include "DetectionResult.h"

class BehaviorAnalyzer;

struct AlertRecord {
    QDateTime timestamp;
    QString   message;
    QString   level; // "warning" or "danger"
};

class AlertManager : public QWidget
{
    Q_OBJECT

public:
    explicit AlertManager(QWidget *parent = nullptr);

    void check(const QMap<int, TrackInfo> &tracks, const BehaviorAnalyzer *analyzer);
    void clearAlerts();

    QVector<AlertRecord> alertHistory() const;

signals:
    void alertTriggered(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void showAlert(const QString &message, const QString &level);
    void hideAlert();

    QLabel *m_alertLabel = nullptr;
    QTimer  m_hideTimer;

    QVector<AlertRecord> m_history;

    int m_noPetFrameCount = 0;
    int m_destructiveFrameCount = 0;

    // Thresholds (in frame counts)
    int m_noPetThreshold = 150;       // ~5s at 30fps
    int m_destructiveThreshold = 60;  // ~2s at 30fps

    bool m_alertVisible = false;
    QString m_currentLevel;
};

#endif // ALERTMANAGER_H
