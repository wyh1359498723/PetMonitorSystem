#include "PetPresenceRecorder.h"
#include <QFile>
#include <QtGlobal>
#include <cmath>

PetPresenceRecorder::PetPresenceRecorder()
{
    setFps(30.0);
    setMinSegmentSeconds(0.5);
}

void PetPresenceRecorder::reset()
{
    m_inSegment = false;
    m_segments.clear();
}

void PetPresenceRecorder::setFps(double fps)
{
    if (fps <= 0.0) fps = 30.0;
    m_fps = fps;
    m_minFrames = qMax(1, static_cast<int>(std::ceil(m_minSegmentSec * m_fps)));
}

void PetPresenceRecorder::setMinSegmentSeconds(double seconds)
{
    if (seconds <= 0.0) seconds = 0.1;
    m_minSegmentSec = seconds;
    m_minFrames = qMax(1, static_cast<int>(std::ceil(m_minSegmentSec * m_fps)));
}

void PetPresenceRecorder::recordFrameRange(int startFrame, int endFrame, bool petDetected)
{
    if (startFrame > endFrame)
        return;
    for (int f = startFrame; f <= endFrame; ++f)
        recordFrameResult(f, petDetected);
}

void PetPresenceRecorder::recordFrameResult(int frameIndex, bool petDetected)
{
    if (petDetected) {
        if (!m_inSegment) {
            m_segStart = m_lastInSeg = frameIndex;
            m_inSegment = true;
        } else {
            if (frameIndex <= m_lastInSeg + 1) {
                m_lastInSeg = frameIndex;
            } else {
                flushOpenSegment();
                m_segStart = m_lastInSeg = frameIndex;
                m_inSegment = true;
            }
        }
    } else {
        if (m_inSegment) {
            flushOpenSegment();
            m_inSegment = false;
        }
    }
}

void PetPresenceRecorder::flushOpenSegment()
{
    const int len = m_lastInSeg - m_segStart + 1;
    if (len < m_minFrames)
        return;

    PetPresenceSegment s;
    s.startFrame = m_segStart;
    s.endFrame   = m_lastInSeg;
    s.startSec = (m_segStart - 1) / m_fps;
    if (s.startSec < 0.0) s.startSec = 0.0;
    s.endSec = static_cast<double>(m_lastInSeg) / m_fps;
    s.durationSec = static_cast<double>(len) / m_fps;

    m_segments.append(s);
}

void PetPresenceRecorder::finalize()
{
    if (m_inSegment) {
        flushOpenSegment();
        m_inSegment = false;
    }
}

bool PetPresenceRecorder::exportToCsv(const QString &filePath) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QByteArray data;
    data.append("\xEF\xBB\xBF"); // UTF-8 BOM，Excel 正确识别中文

    QString header = QStringLiteral("序号,开始时间(秒),结束时间(秒),持续时长(秒),开始帧,结束帧,连续帧数\n");
    data.append(header.toUtf8());

    for (int i = 0; i < m_segments.size(); ++i) {
        const PetPresenceSegment &s = m_segments[i];
        QString line = QStringLiteral("%1,%2,%3,%4,%5,%6,%7\n")
                           .arg(i + 1)
                           .arg(s.startSec, 0, 'f', 3)
                           .arg(s.endSec, 0, 'f', 3)
                           .arg(s.durationSec, 0, 'f', 3)
                           .arg(s.startFrame)
                           .arg(s.endFrame)
                           .arg(s.endFrame - s.startFrame + 1);
        data.append(line.toUtf8());
    }

    return f.write(data) == data.size();
}
