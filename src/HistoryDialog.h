#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>

class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);

private slots:
    void onSessionSelected(int row, int column);
    void onDeleteSelected();

private:
    void loadSessions();
    void showSessionDetail(qint64 sessionId);
    void showAllTimeBehavior();
    void clearDetailView();

    QPushButton  *m_btnDelete = nullptr;
    QTableWidget *m_sessionTable = nullptr;
    QTableWidget *m_presenceTable = nullptr;
    QTableWidget *m_alertTable = nullptr;
    QChartView   *m_behaviorChartView = nullptr;
    QChartView   *m_allTimeChartView = nullptr;
};

#endif // HISTORYDIALOG_H
