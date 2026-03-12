#include <QApplication>
#include <QMetaType>
#include <QVector>
#include <QMap>
#include <QPair>
#include <opencv2/core.hpp>
#include "DetectionResult.h"
#include "MainWindow.h"

using DetectionList = QVector<DetectionResult>;
using TrackMap      = QMap<int, TrackInfo>;
using BehaviorMap   = QMap<PetBehavior, int>;
using TimelineList  = QVector<QPair<int, double>>;

Q_DECLARE_METATYPE(cv::Mat)
Q_DECLARE_METATYPE(DetectionList)
Q_DECLARE_METATYPE(TrackMap)
Q_DECLARE_METATYPE(BehaviorMap)
Q_DECLARE_METATYPE(TimelineList)

int main(int argc, char *argv[])
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<DetectionList>("QVector<DetectionResult>");
    qRegisterMetaType<TrackMap>("QMap<int,TrackInfo>");
    qRegisterMetaType<BehaviorMap>("QMap<PetBehavior,int>");
    qRegisterMetaType<TimelineList>("QVector<QPair<int,double>>");

    QApplication app(argc, argv);
    app.setApplicationName("PetMonitorSystem");
    app.setOrganizationName("GraduationProject");

    MainWindow w;
    w.show();

    return app.exec();
}
