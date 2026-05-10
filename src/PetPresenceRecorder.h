#ifndef PETPRESENCERECORDER_H
#define PETPRESENCERECORDER_H

#include <QString>
#include <QVector>

/// 画面中检测到宠物的一段连续时间（仅保留达到最小时长的区段）
struct PetPresenceSegment {
    int    startFrame = 0;
    int    endFrame   = 0;
    double startSec   = 0.0;
    double endSec     = 0.0;
    double durationSec = 0.0;
};

class PetPresenceRecorder
{
public:
    PetPresenceRecorder();

    void reset();
    void setFps(double fps);
    /// 最小区段时长（秒），换算为最少帧数，过短的连续段会被丢弃
    void setMinSegmentSeconds(double seconds);

    /// 每帧推理结果：petDetected = 是否检测到猫/狗
    void recordFrameResult(int frameIndex, bool petDetected);
    /// 将 [startFrame, endFrame] 闭区间标为同一检测结果（用于整段扫描跳帧块）
    void recordFrameRange(int startFrame, int endFrame, bool petDetected);
    /// 播放结束或导出前调用，闭合未结束的区段
    void finalize();

    QVector<PetPresenceSegment> segments() const { return m_segments; }
    int segmentCount() const { return m_segments.size(); }

    /// 导出为 CSV（UTF-8 BOM），可用 Excel 直接打开
    bool exportToCsv(const QString &filePath) const;

private:
    void flushOpenSegment();

    double m_fps = 30.0;
    double m_minSegmentSec = 0.5;
    int    m_minFrames = 15;

    bool   m_inSegment = false;
    int    m_segStart = 0;
    int    m_lastInSeg = 0;

    QVector<PetPresenceSegment> m_segments;
};

#endif // PETPRESENCERECORDER_H
