#include "AlertManager.h"
#include "BehaviorAnalyzer.h"
#include <QPainter>
#include <QPropertyAnimation>

AlertManager::AlertManager(QWidget *parent)
    : QWidget(parent)
{
    m_alertLabel = new QLabel(this);
    m_alertLabel->setAlignment(Qt::AlignCenter);
    m_alertLabel->setStyleSheet(
        "QLabel { color: white; font-size: 14px; font-weight: bold; "
        "font-family: 'Microsoft YaHei'; padding: 8px; }");
    m_alertLabel->hide();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_alertLabel);

    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &AlertManager::hideAlert);
}

void AlertManager::check(const QMap<int, TrackInfo> &tracks, const BehaviorAnalyzer * /*analyzer*/)
{
    bool hasPet = !tracks.isEmpty();
    bool hasDestructive = false;

    for (auto it = tracks.constBegin(); it != tracks.constEnd(); ++it) {
        if (it.value().currentBehavior == PetBehavior::Destructive) {
            hasDestructive = true;
            break;
        }
    }

    // Check: no pet detected for extended period
    if (!hasPet) {
        m_noPetFrameCount++;
        if (m_noPetFrameCount == m_noPetThreshold) {
            showAlert(QStringLiteral("⚠ 警告：长时间未检测到宠物，可能已离开视野！"), "warning");
        }
    } else {
        m_noPetFrameCount = 0;
    }

    // Check: destructive behavior
    if (hasDestructive) {
        m_destructiveFrameCount++;
        if (m_destructiveFrameCount == m_destructiveThreshold) {
            showAlert(QStringLiteral("🚨 紧急：检测到疑似拆家行为！请立即查看！"), "danger");
        }
    } else {
        m_destructiveFrameCount = 0;
    }
}

void AlertManager::showAlert(const QString &message, const QString &level)
{
    m_currentLevel = level;
    m_alertVisible = true;

    if (level == "danger") {
        setStyleSheet("background-color: #dc3545;");
    } else {
        setStyleSheet("background-color: #ffc107;");
    }

    m_alertLabel->setText(message);
    m_alertLabel->show();
    setFixedHeight(40);

    // Record to history
    AlertRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.message = message;
    record.level = level;
    m_history.append(record);

    emit alertTriggered(message);

    m_hideTimer.start(5000);
}

void AlertManager::hideAlert()
{
    m_alertVisible = false;
    m_alertLabel->hide();
    setFixedHeight(0);
    setStyleSheet("");
}

void AlertManager::clearAlerts()
{
    hideAlert();
    m_history.clear();
    m_noPetFrameCount = 0;
    m_destructiveFrameCount = 0;
}

QVector<AlertRecord> AlertManager::alertHistory() const
{
    return m_history;
}

void AlertManager::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
}
