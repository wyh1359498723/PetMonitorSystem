#ifndef STATISTICSWIDGET_H
#define STATISTICSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QPair>
#include "DetectionResult.h"

#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class StatisticsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatisticsWidget(QWidget *parent = nullptr);

    void updateStatistics(const QMap<PetBehavior, int> &durations,
                          const QVector<QPair<int, double>> &timeline);
    void resetStatistics();

private:
    void setupCharts();

    QChartView  *m_pieChartView  = nullptr;
    QChartView  *m_lineChartView = nullptr;
    QPieSeries  *m_pieSeries     = nullptr;
    QLineSeries *m_lineSeries    = nullptr;
    QValueAxis  *m_axisX         = nullptr;
    QValueAxis  *m_axisY         = nullptr;
    QChart      *m_pieChart      = nullptr;
    QChart      *m_lineChart     = nullptr;
};

#endif // STATISTICSWIDGET_H
