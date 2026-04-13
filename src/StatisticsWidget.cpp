#include "StatisticsWidget.h"
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QPieSlice>
#include <algorithm>

static QColor behaviorColor(PetBehavior b)
{
    switch (b) {
    case PetBehavior::Sleeping:    return QColor(100, 149, 237);
    case PetBehavior::Sitting:     return QColor(60, 179, 113);
    case PetBehavior::Walking:     return QColor(255, 165, 0);
    case PetBehavior::Running:     return QColor(255, 69, 0);
    case PetBehavior::Eating:      return QColor(50, 205, 50);
    case PetBehavior::Destructive: return QColor(220, 20, 60);
    default:                       return QColor(180, 180, 180);
    }
}

StatisticsWidget::StatisticsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupCharts();
}

void StatisticsWidget::setupCharts()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // --- Pie chart (no animations for performance) ---
    m_pieSeries = new QPieSeries();
    m_pieSeries->setHoleSize(0.35);

    m_pieChart = new QChart();
    m_pieChart->addSeries(m_pieSeries);
    m_pieChart->setTitle(QStringLiteral("行为时间分布"));
    m_pieChart->setTheme(QChart::ChartThemeDark);
    m_pieChart->setAnimationOptions(QChart::NoAnimation);
    m_pieChart->legend()->setAlignment(Qt::AlignRight);
    m_pieChart->setMargins(QMargins(2, 2, 2, 2));

    m_pieChartView = new QChartView(m_pieChart);
    m_pieChartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_pieChartView, 1);

    // --- Line chart (no animations) ---
    m_lineSeries = new QLineSeries();
    m_lineSeries->setName(QStringLiteral("活动量"));

    m_lineChart = new QChart();
    m_lineChart->addSeries(m_lineSeries);
    m_lineChart->setTitle(QStringLiteral("活动量趋势"));
    m_lineChart->setTheme(QChart::ChartThemeDark);
    m_lineChart->setAnimationOptions(QChart::NoAnimation);
    m_lineChart->setMargins(QMargins(2, 2, 2, 2));

    m_axisX = new QValueAxis();
    m_axisX->setTitleText(QStringLiteral("帧"));
    m_axisX->setLabelFormat("%d");

    m_axisY = new QValueAxis();
    m_axisY->setTitleText(QStringLiteral("速度"));
    m_axisY->setMin(0);

    m_lineChart->addAxis(m_axisX, Qt::AlignBottom);
    m_lineChart->addAxis(m_axisY, Qt::AlignLeft);
    m_lineSeries->attachAxis(m_axisX);
    m_lineSeries->attachAxis(m_axisY);

    m_lineChartView = new QChartView(m_lineChart);
    m_lineChartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_lineChartView, 1);
}

void StatisticsWidget::updateStatistics(const QMap<PetBehavior, int> &durations,
                                         const QVector<QPair<int, double>> &timeline)
{
    // --- Pie chart: rebuild only slices ---
    m_pieSeries->clear();
    int total = 0;
    for (auto it = durations.constBegin(); it != durations.constEnd(); ++it) {
        total += it.value();
    }

    if (total > 0) {
        for (auto it = durations.constBegin(); it != durations.constEnd(); ++it) {
            if (it.value() == 0) continue;
            QString label = behaviorToString(it.key());
            auto *slice = m_pieSeries->append(label, it.value());
            slice->setColor(behaviorColor(it.key()));
            double pct = 100.0 * it.value() / total;
            if (pct > 5.0) {
                slice->setLabelVisible(true);
                slice->setLabel(QStringLiteral("%1 %2%")
                                    .arg(label)
                                    .arg(pct, 0, 'f', 1));
            }
        }
    }

    // --- Line chart: batch replace instead of clear+append loop ---
    qsizetype startIdx = std::max(qsizetype(0), timeline.size() - 300);
    double maxY = 1.0;

    QList<QPointF> points;
    points.reserve(timeline.size() - startIdx);

    for (qsizetype i = startIdx; i < timeline.size(); ++i) {
        double x = timeline[i].first;
        double y = timeline[i].second;
        points.append(QPointF(x, y));
        if (y > maxY) maxY = y;
    }

    m_lineSeries->replace(points);

    if (!timeline.isEmpty()) {
        m_axisX->setRange(timeline[startIdx].first, timeline.last().first);
        m_axisY->setRange(0, maxY * 1.2);
    }
}

void StatisticsWidget::resetStatistics()
{
    m_pieSeries->clear();
    m_lineSeries->clear();
    m_axisX->setRange(0, 100);
    m_axisY->setRange(0, 50);
}
