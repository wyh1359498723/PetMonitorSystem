#include "HistoryDialog.h"
#include "DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QtCharts/QPieSlice>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>

static QColor behaviorColor(const QString &b)
{
    if (b == QStringLiteral("睡眠"))   return QColor(100, 149, 237);
    if (b == QStringLiteral("静坐"))   return QColor(60, 179, 113);
    if (b == QStringLiteral("走动"))   return QColor(255, 165, 0);
    if (b == QStringLiteral("奔跑"))   return QColor(255, 69, 0);
    if (b == QStringLiteral("进食"))   return QColor(50, 205, 50);
    if (b == QStringLiteral("拆家"))   return QColor(220, 20, 60);
    return QColor(180, 180, 180);
}

static const char *kDialogStyle = R"(
    HistoryDialog {
        background-color: #0c0c12;
    }

    QLabel#histTitle {
        color: #f8fafc;
        font-size: 20px;
        font-weight: 700;
        padding: 4px 0;
    }
    QLabel#histSubtitle {
        color: #a5b4fc;
        font-size: 12px;
        font-weight: 400;
    }
    QLabel#sectionLabel {
        color: #c7d2fe;
        font-size: 13px;
        font-weight: 600;
        padding: 2px 0;
    }

    QGroupBox {
        color: #e4e4e7;
        font-weight: 600;
        border: 1px solid #2d2d3d;
        border-radius: 10px;
        margin-top: 14px;
        padding: 14px 10px 10px 10px;
        background-color: #16161f;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        subcontrol-position: top left;
        left: 12px;
        padding: 0 8px;
        color: #a5b4fc;
    }

    QTableWidget {
        background-color: #13131a;
        alternate-background-color: #1a1a24;
        color: #e4e4e7;
        border: 1px solid #2d2d3d;
        border-radius: 8px;
        gridline-color: #252532;
        selection-background-color: #312e81;
        selection-color: #e0e7ff;
        font-size: 12px;
    }
    QTableWidget::item {
        padding: 4px 8px;
        border: none;
    }
    QTableWidget::item:selected {
        background-color: #312e81;
        color: #e0e7ff;
    }
    QHeaderView::section {
        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
            stop:0 #1e1b4b, stop:1 #16161f);
        color: #c7d2fe;
        font-weight: 600;
        font-size: 12px;
        padding: 6px 8px;
        border: none;
        border-bottom: 2px solid #4338ca;
        border-right: 1px solid #252532;
    }
    QHeaderView::section:last {
        border-right: none;
    }

    QPushButton#btnDelete {
        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
            stop:0 #dc2626, stop:1 #b91c1c);
        color: #ffffff;
        border: none;
        border-radius: 8px;
        padding: 8px 18px;
        font-weight: 600;
        font-size: 13px;
    }
    QPushButton#btnDelete:hover {
        background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
            stop:0 #ef4444, stop:1 #dc2626);
    }
    QPushButton#btnDelete:pressed {
        background: #991b1b;
    }

    QPushButton#btnRefresh {
        background-color: #27272f;
        color: #e4e4e7;
        border: 1px solid #3f3f46;
        border-radius: 8px;
        padding: 8px 18px;
        font-weight: 500;
        font-size: 13px;
    }
    QPushButton#btnRefresh:hover {
        background-color: #3f3f46;
        border-color: #6366f1;
    }

    QSplitter::handle {
        background-color: #1f1f2a;
        width: 5px;
        border-radius: 2px;
    }
    QSplitter::handle:hover {
        background-color: #4338ca;
    }

    QScrollArea {
        border: none;
        background-color: transparent;
    }

    QChartView {
        background-color: transparent;
        border: none;
    }
)";

HistoryDialog::HistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("历史数据"));
    resize(1200, 750);
    setStyleSheet(kDialogStyle);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    // ---- Top header bar ----
    auto *headerBar = new QWidget;
    headerBar->setFixedHeight(52);
    headerBar->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #1e1b4b, stop:0.5 #312e81, stop:1 #1e1b4b);"
        "border-radius: 10px; border-bottom: 1px solid #4338ca;");
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(1);
    auto *lblTitle = new QLabel(QStringLiteral("历史数据中心"));
    lblTitle->setObjectName("histTitle");
    auto *lblSub = new QLabel(QStringLiteral("会话记录 · 行为分析 · 告警追踪"));
    lblSub->setObjectName("histSubtitle");
    titleCol->addWidget(lblTitle);
    titleCol->addWidget(lblSub);
    headerLayout->addLayout(titleCol);
    headerLayout->addStretch();

    m_btnDelete = new QPushButton(QStringLiteral("删除选中"));
    m_btnDelete->setObjectName("btnDelete");
    m_btnDelete->setFixedHeight(34);
    auto *btnRefresh = new QPushButton(QStringLiteral("刷新"));
    btnRefresh->setObjectName("btnRefresh");
    btnRefresh->setFixedHeight(34);
    headerLayout->addWidget(btnRefresh);
    headerLayout->addWidget(m_btnDelete);

    root->addWidget(headerBar);

    // ---- Body splitter ----
    auto *splitter = new QSplitter(Qt::Horizontal);

    // ========== LEFT PANEL ==========
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto *lblSessions = new QLabel(QStringLiteral("分析会话记录"));
    lblSessions->setObjectName("sectionLabel");
    leftLayout->addWidget(lblSessions);

    m_sessionTable = new QTableWidget;
    m_sessionTable->setColumnCount(5);
    m_sessionTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("数据源"),
        QStringLiteral("开始时间"),
        QStringLiteral("结束时间"),
        QStringLiteral("帧数")
    });
    m_sessionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_sessionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessionTable->setAlternatingRowColors(true);
    m_sessionTable->verticalHeader()->setVisible(false);
    m_sessionTable->horizontalHeader()->setStretchLastSection(true);
    m_sessionTable->setShowGrid(false);
    m_sessionTable->verticalHeader()->setDefaultSectionSize(32);
    leftLayout->addWidget(m_sessionTable, 3);

    // All-time behavior chart
    auto *allTimeGroup = new QGroupBox(QStringLiteral("历史行为总览"));
    auto *allTimeLayout = new QVBoxLayout(allTimeGroup);
    allTimeLayout->setContentsMargins(6, 16, 6, 6);
    m_allTimeChartView = new QChartView;
    m_allTimeChartView->setRenderHint(QPainter::Antialiasing);
    m_allTimeChartView->setMinimumHeight(180);
    allTimeLayout->addWidget(m_allTimeChartView);
    leftLayout->addWidget(allTimeGroup, 2);

    splitter->addWidget(leftWidget);

    // ========== RIGHT PANEL (scrollable) ==========
    auto *rightScroll = new QScrollArea;
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);

    auto *rightWidget = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    // Behavior pie chart
    auto *behGroup = new QGroupBox(QStringLiteral("本次行为分布"));
    auto *behLayout = new QVBoxLayout(behGroup);
    behLayout->setContentsMargins(6, 16, 6, 6);
    m_behaviorChartView = new QChartView;
    m_behaviorChartView->setRenderHint(QPainter::Antialiasing);
    m_behaviorChartView->setMinimumHeight(220);
    behLayout->addWidget(m_behaviorChartView);
    rightLayout->addWidget(behGroup);

    // Presence segments table
    auto *presGroup = new QGroupBox(QStringLiteral("宠物出现时段"));
    auto *presLayout = new QVBoxLayout(presGroup);
    presLayout->setContentsMargins(6, 16, 6, 6);
    m_presenceTable = new QTableWidget;
    m_presenceTable->setColumnCount(5);
    m_presenceTable->setHorizontalHeaderLabels({
        QStringLiteral("开始(秒)"),
        QStringLiteral("结束(秒)"),
        QStringLiteral("持续(秒)"),
        QStringLiteral("开始帧"),
        QStringLiteral("结束帧")
    });
    m_presenceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_presenceTable->setAlternatingRowColors(true);
    m_presenceTable->verticalHeader()->setVisible(false);
    m_presenceTable->horizontalHeader()->setStretchLastSection(true);
    m_presenceTable->setShowGrid(false);
    m_presenceTable->verticalHeader()->setDefaultSectionSize(30);
    m_presenceTable->setMinimumHeight(140);
    presLayout->addWidget(m_presenceTable);
    rightLayout->addWidget(presGroup);

    // Alert history table
    auto *alertGroup = new QGroupBox(QStringLiteral("告警记录"));
    auto *alertLayout = new QVBoxLayout(alertGroup);
    alertLayout->setContentsMargins(6, 16, 6, 6);
    m_alertTable = new QTableWidget;
    m_alertTable->setColumnCount(3);
    m_alertTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("级别"),
        QStringLiteral("内容")
    });
    m_alertTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alertTable->setAlternatingRowColors(true);
    m_alertTable->verticalHeader()->setVisible(false);
    m_alertTable->horizontalHeader()->setStretchLastSection(true);
    m_alertTable->setShowGrid(false);
    m_alertTable->verticalHeader()->setDefaultSectionSize(30);
    m_alertTable->setMinimumHeight(120);
    alertLayout->addWidget(m_alertTable);
    rightLayout->addWidget(alertGroup);

    rightLayout->addStretch();
    rightScroll->setWidget(rightWidget);

    splitter->addWidget(rightScroll);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setChildrenCollapsible(false);

    root->addWidget(splitter, 1);

    // ---- Connections ----
    connect(m_sessionTable, &QTableWidget::cellClicked,
            this, &HistoryDialog::onSessionSelected);
    connect(m_btnDelete, &QPushButton::clicked,
            this, &HistoryDialog::onDeleteSelected);
    connect(btnRefresh, &QPushButton::clicked, this, [this]() {
        loadSessions();
        showAllTimeBehavior();
    });

    // Init
    clearDetailView();
    loadSessions();
    showAllTimeBehavior();
}

void HistoryDialog::loadSessions()
{
    auto sessions = DatabaseManager::instance().allSessions();
    m_sessionTable->setRowCount(sessions.size());

    for (int i = 0; i < sessions.size(); ++i) {
        const auto &s = sessions[i];
        auto *idItem = new QTableWidgetItem(QString::number(s.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_sessionTable->setItem(i, 0, idItem);

        m_sessionTable->setItem(i, 1, new QTableWidgetItem(s.source));

        m_sessionTable->setItem(i, 2, new QTableWidgetItem(
            s.startTime.toString("yyyy-MM-dd HH:mm:ss")));

        m_sessionTable->setItem(i, 3, new QTableWidgetItem(
            s.endTime.isValid() ? s.endTime.toString("yyyy-MM-dd HH:mm:ss")
                                : QStringLiteral("进行中…")));

        auto *frameItem = new QTableWidgetItem(QString::number(s.totalFrames));
        frameItem->setTextAlignment(Qt::AlignCenter);
        m_sessionTable->setItem(i, 4, frameItem);
    }

    m_sessionTable->resizeColumnsToContents();
    if (m_sessionTable->columnWidth(1) > 200)
        m_sessionTable->setColumnWidth(1, 200);
}

void HistoryDialog::onSessionSelected(int row, int /*column*/)
{
    if (row < 0 || row >= m_sessionTable->rowCount()) return;
    qint64 sessionId = m_sessionTable->item(row, 0)->text().toLongLong();
    showSessionDetail(sessionId);
}

void HistoryDialog::showSessionDetail(qint64 sessionId)
{
    auto &db = DatabaseManager::instance();

    // ---- Behavior pie chart ----
    auto behaviors = db.behaviorForSession(sessionId);
    auto *pie = new QPieSeries;
    pie->setHoleSize(0.40);
    for (const auto &b : behaviors) {
        auto *slice = pie->append(
            QStringLiteral("%1 %2%").arg(b.behavior).arg(b.percentage, 0, 'f', 1),
            b.frameDuration);
        slice->setColor(behaviorColor(b.behavior));
        slice->setBorderColor(QColor(12, 12, 18));
        slice->setBorderWidth(2);
        if (b.percentage > 5.0) {
            slice->setLabelVisible(true);
            slice->setLabelColor(QColor(228, 228, 231));
            slice->setLabelFont(QFont("Microsoft YaHei UI", 9, QFont::Bold));
        }
    }

    auto *chart = new QChart;
    chart->addSeries(pie);
    chart->setTitle(QStringLiteral("会话 #%1 — 行为分布").arg(sessionId));
    chart->setTheme(QChart::ChartThemeDark);
    chart->setBackgroundBrush(QBrush(QColor(22, 22, 31)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(22, 22, 31)));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationDuration(300);
    chart->legend()->setAlignment(Qt::AlignRight);
    chart->legend()->setLabelColor(QColor(196, 196, 210));
    chart->legend()->setFont(QFont("Microsoft YaHei UI", 9));
    chart->setTitleFont(QFont("Microsoft YaHei UI", 11, QFont::Bold));
    chart->setTitleBrush(QBrush(QColor(165, 180, 252)));
    m_behaviorChartView->setChart(chart);

    // ---- Presence table ----
    auto presence = db.presenceForSession(sessionId);
    m_presenceTable->setRowCount(presence.size());
    for (int i = 0; i < presence.size(); ++i) {
        const auto &p = presence[i];
        auto setCell = [&](int col, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            m_presenceTable->setItem(i, col, item);
        };
        setCell(0, QString::number(p.startSec, 'f', 2));
        setCell(1, QString::number(p.endSec, 'f', 2));
        setCell(2, QString::number(p.durationSec, 'f', 2));
        setCell(3, QString::number(p.startFrame));
        setCell(4, QString::number(p.endFrame));
    }
    m_presenceTable->resizeColumnsToContents();

    // ---- Alert table ----
    auto alerts = db.alertsForSession(sessionId);
    m_alertTable->setRowCount(alerts.size());
    for (int i = 0; i < alerts.size(); ++i) {
        const auto &a = alerts[i];
        auto *timeItem = new QTableWidgetItem(a.timestamp.toString("HH:mm:ss"));
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_alertTable->setItem(i, 0, timeItem);

        auto *levelItem = new QTableWidgetItem(a.level);
        levelItem->setTextAlignment(Qt::AlignCenter);
        if (a.level == "danger")
            levelItem->setForeground(QColor(239, 68, 68));
        else
            levelItem->setForeground(QColor(251, 191, 36));
        m_alertTable->setItem(i, 1, levelItem);

        m_alertTable->setItem(i, 2, new QTableWidgetItem(a.message));
    }
    m_alertTable->resizeColumnsToContents();
}

void HistoryDialog::showAllTimeBehavior()
{
    auto summary = DatabaseManager::instance().behaviorSummaryAllTime();
    if (summary.isEmpty()) {
        auto *chart = new QChart;
        chart->setTitle(QStringLiteral("暂无历史数据"));
        chart->setTheme(QChart::ChartThemeDark);
        chart->setBackgroundBrush(QBrush(QColor(22, 22, 31)));
        chart->setTitleFont(QFont("Microsoft YaHei UI", 11, QFont::Bold));
        chart->setTitleBrush(QBrush(QColor(165, 180, 252)));
        m_allTimeChartView->setChart(chart);
        return;
    }

    auto *barSet = new QBarSet(QStringLiteral("累计帧数"));
    barSet->setColor(QColor(99, 102, 241));
    barSet->setBorderColor(QColor(129, 140, 248));
    QStringList categories;
    double maxVal = 0;
    for (auto it = summary.constBegin(); it != summary.constEnd(); ++it) {
        categories << it.key();
        *barSet << it.value();
        if (it.value() > maxVal) maxVal = it.value();
    }

    auto *series = new QBarSeries;
    series->append(barSet);
    series->setBarWidth(0.55);

    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("各行为历史累计（帧）"));
    chart->setTheme(QChart::ChartThemeDark);
    chart->setBackgroundBrush(QBrush(QColor(22, 22, 31)));
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(22, 22, 31)));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationDuration(300);
    chart->setTitleFont(QFont("Microsoft YaHei UI", 11, QFont::Bold));
    chart->setTitleBrush(QBrush(QColor(165, 180, 252)));

    auto *axisX = new QBarCategoryAxis;
    axisX->append(categories);
    axisX->setLabelsColor(QColor(196, 196, 210));
    axisX->setGridLineVisible(false);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis;
    axisY->setRange(0, maxVal * 1.15);
    axisY->setTitleText(QStringLiteral("帧数"));
    axisY->setTitleBrush(QBrush(QColor(165, 180, 252)));
    axisY->setLabelsColor(QColor(161, 161, 170));
    axisY->setGridLineColor(QColor(37, 37, 50));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(false);
    m_allTimeChartView->setChart(chart);
}

void HistoryDialog::clearDetailView()
{
    m_presenceTable->setRowCount(0);
    m_alertTable->setRowCount(0);

    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("← 点击左侧会话查看详情"));
    chart->setTheme(QChart::ChartThemeDark);
    chart->setBackgroundBrush(QBrush(QColor(22, 22, 31)));
    chart->setTitleFont(QFont("Microsoft YaHei UI", 11));
    chart->setTitleBrush(QBrush(QColor(113, 113, 122)));
    m_behaviorChartView->setChart(chart);
}

void HistoryDialog::onDeleteSelected()
{
    auto selectedRows = m_sessionTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在左侧表格中选择要删除的记录。"));
        return;
    }

    int count = selectedRows.size();
    auto ret = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定删除选中的 %1 条会话记录及其所有关联数据？\n此操作不可撤销。").arg(count),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes) return;

    QVector<qint64> ids;
    for (const auto &idx : selectedRows) {
        auto *item = m_sessionTable->item(idx.row(), 0);
        if (item) ids.append(item->text().toLongLong());
    }

    for (qint64 id : ids)
        DatabaseManager::instance().deleteSession(id);

    clearDetailView();
    loadSessions();
    showAllTimeBehavior();
}
