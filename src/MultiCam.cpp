#include "MultiCam.h"
#include "WaveformGenerator.h"
#include <QFileInfo>
#include <algorithm>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
}

MultiCamSession::MultiCamSession(QObject *parent)
    : QObject(parent) {}

void MultiCamSession::addSource(const QString &filePath, const QString &label)
{
    CameraSource src;
    src.filePath = filePath;
    src.label = label.isEmpty() ?
        QString("Camera %1").arg(m_sources.size() + 1) : label;

    // Get duration
    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, filePath.toUtf8().constData(), nullptr, nullptr) == 0) {
        if (avformat_find_stream_info(fmt, nullptr) >= 0)
            src.duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
        avformat_close_input(&fmt);
    }

    m_sources.append(src);
    emit sourcesChanged();
}

void MultiCamSession::removeSource(int index)
{
    if (index < 0 || index >= m_sources.size()) return;
    m_sources.removeAt(index);

    // Remove cuts referencing this source and adjust indices
    QVector<CameraCut> validCuts;
    for (auto &cut : m_cuts) {
        if (cut.cameraIndex == index) continue;
        if (cut.cameraIndex > index) cut.cameraIndex--;
        validCuts.append(cut);
    }
    m_cuts = validCuts;

    emit sourcesChanged();
    emit cutsChanged();
}

void MultiCamSession::setSyncOffset(int sourceIndex, double offset)
{
    if (sourceIndex >= 0 && sourceIndex < m_sources.size()) {
        m_sources[sourceIndex].syncOffset = offset;
        emit sourcesChanged();
    }
}

void MultiCamSession::autoSyncByAudio()
{
    if (m_sources.size() < 2) return;

    // Generate waveforms for all sources
    QVector<WaveformData> waveforms;
    for (const auto &src : m_sources)
        waveforms.append(WaveformGenerator::generate(src.filePath, 100));

    // Cross-correlate each source against the first (reference)
    const auto &ref = waveforms[0];
    if (ref.isEmpty()) return;

    for (int i = 1; i < waveforms.size(); ++i) {
        const auto &cmp = waveforms[i];
        if (cmp.isEmpty()) continue;

        // Simple cross-correlation to find best offset
        int maxShift = qMin(ref.peaks.size(), cmp.peaks.size()) / 2;
        double bestCorr = -1.0;
        int bestOffset = 0;

        for (int shift = -maxShift; shift <= maxShift; shift += 5) { // step of 5 for speed
            double corr = 0.0;
            int count = 0;
            for (int j = 0; j < ref.peaks.size() && j + shift >= 0 && j + shift < cmp.peaks.size(); ++j) {
                corr += ref.peaks[j] * cmp.peaks[j + shift];
                ++count;
            }
            if (count > 0) corr /= count;
            if (corr > bestCorr) {
                bestCorr = corr;
                bestOffset = shift;
            }
        }

        // Refine around best offset
        for (int shift = bestOffset - 5; shift <= bestOffset + 5; ++shift) {
            double corr = 0.0;
            int count = 0;
            for (int j = 0; j < ref.peaks.size() && j + shift >= 0 && j + shift < cmp.peaks.size(); ++j) {
                corr += ref.peaks[j] * cmp.peaks[j + shift];
                ++count;
            }
            if (count > 0) corr /= count;
            if (corr > bestCorr) {
                bestCorr = corr;
                bestOffset = shift;
            }
        }

        double offsetSeconds = static_cast<double>(bestOffset) / ref.peaksPerSecond;
        m_sources[i].syncOffset = offsetSeconds;
    }

    emit syncCompleted();
    emit sourcesChanged();
}

void MultiCamSession::switchToCamera(int cameraIndex, double time)
{
    if (cameraIndex < 0 || cameraIndex >= m_sources.size()) return;

    // Close any current cut at this time
    for (auto &cut : m_cuts) {
        if (cut.endTime > time && cut.startTime < time) {
            cut.endTime = time;
        }
    }

    // Remove future cuts
    m_cuts.erase(
        std::remove_if(m_cuts.begin(), m_cuts.end(),
            [time](const CameraCut &c) { return c.startTime >= time; }),
        m_cuts.end());

    // Add new cut
    CameraCut cut;
    cut.cameraIndex = cameraIndex;
    cut.startTime = time;
    cut.endTime = totalDuration(); // until end or next switch
    m_cuts.append(cut);

    sortCuts();
    emit cutsChanged();
}

void MultiCamSession::addCut(int cameraIndex, double startTime, double endTime)
{
    CameraCut cut;
    cut.cameraIndex = cameraIndex;
    cut.startTime = startTime;
    cut.endTime = endTime;
    m_cuts.append(cut);
    sortCuts();
    emit cutsChanged();
}

void MultiCamSession::removeCut(int index)
{
    if (index >= 0 && index < m_cuts.size()) {
        m_cuts.removeAt(index);
        emit cutsChanged();
    }
}

int MultiCamSession::activeCameraAt(double time) const
{
    for (const auto &cut : m_cuts) {
        if (time >= cut.startTime && time < cut.endTime)
            return cut.cameraIndex;
    }
    return 0; // default to first camera
}

int MultiCamSession::gridColumns() const
{
    int n = m_sources.size();
    if (n <= 1) return 1;
    if (n <= 4) return 2;
    if (n <= 9) return 3;
    return 4;
}

int MultiCamSession::gridRows() const
{
    int cols = gridColumns();
    return (m_sources.size() + cols - 1) / cols;
}

QVector<MultiCamSession::EditSegment> MultiCamSession::generateEditList() const
{
    QVector<EditSegment> segments;
    for (const auto &cut : m_cuts) {
        if (cut.cameraIndex >= m_sources.size()) continue;
        EditSegment seg;
        seg.cameraIndex = cut.cameraIndex;
        seg.timelineStart = cut.startTime;
        seg.timelineEnd = cut.endTime;
        seg.sourceStart = cut.startTime + m_sources[cut.cameraIndex].syncOffset;
        seg.sourceEnd = cut.endTime + m_sources[cut.cameraIndex].syncOffset;
        segments.append(seg);
    }
    return segments;
}

double MultiCamSession::totalDuration() const
{
    double maxDur = 0.0;
    for (const auto &src : m_sources) {
        double effective = src.duration - src.syncOffset;
        maxDur = qMax(maxDur, effective);
    }
    return maxDur;
}

void MultiCamSession::sortCuts()
{
    std::sort(m_cuts.begin(), m_cuts.end(),
        [](const CameraCut &a, const CameraCut &b) { return a.startTime < b.startTime; });
}
