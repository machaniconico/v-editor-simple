#include "AudioMixer.h"

#include <QtGlobal>
#include <QDebug>
#include <QtMath>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

// Per-entry decoder context + ring buffer. Kept out of the header so FFmpeg
// types stay private. US-3 will fill in the open/decode/swresample logic.
struct AudioDecoderEntry {
    PlaybackEntry entry;
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwrContext *swrCtx = nullptr;
    int audioStreamIdx = -1;
    QByteArray ring;            // resampled s16 stereo samples
    int64_t entryReadCursorUs = 0;
    bool eof = false;
};

class MixerIODevice : public QIODevice {
public:
    explicit MixerIODevice(AudioMixer *mixer) : m_mixer(mixer) {}
protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return -1; }
private:
    AudioMixer *m_mixer;
};

qint64 MixerIODevice::readData(char *data, qint64 maxlen) {
    // US-2 skeleton: emit silence and advance the master clock so callers
    // observe a monotonically increasing audible position. US-3 replaces
    // this with the real ring-mix path.
    if (!data || maxlen <= 0) return 0;
    std::memset(data, 0, static_cast<size_t>(maxlen));
    if (m_mixer && m_mixer->m_playing.load(std::memory_order_acquire)) {
        const int frames = static_cast<int>(maxlen / AudioMixer::kBytesPerFrame);
        const int64_t deltaUs = static_cast<int64_t>(frames) * 1'000'000
                                / AudioMixer::kSampleRateHz;
        m_mixer->m_writeCursorUs.fetch_add(deltaUs, std::memory_order_release);
    }
    return maxlen;
}

AudioMixer::AudioMixer(QObject *parent) : QObject(parent) {
    m_format.setSampleRate(kSampleRateHz);
    m_format.setChannelCount(kChannels);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioMixer::~AudioMixer() {
    stop();
    releaseAllEntries();
    if (m_io) {
        m_io->close();
        delete m_io;
        m_io = nullptr;
    }
    if (m_sink) {
        delete m_sink;
        m_sink = nullptr;
    }
}

void AudioMixer::setSequence(const QVector<PlaybackEntry> &entries) {
    QMutexLocker lock(&m_controlMutex);
    int maxTrack = -1;
    for (const auto &e : entries) maxTrack = qMax(maxTrack, e.sourceTrack);
    if (m_trackStates.size() < maxTrack + 1)
        m_trackStates.resize(maxTrack + 1);
    // US-3 will (re)open decoders here.
    recomputeEffectiveGains();
}

void AudioMixer::seekTo(int64_t timelineUs) {
    QMutexLocker lock(&m_controlMutex);
    m_writeCursorUs.store(timelineUs, std::memory_order_release);
    // US-3 will perform real avformat_seek_file + ring flush per entry.
}

void AudioMixer::play() {
    m_playing.store(true, std::memory_order_release);
}

void AudioMixer::pause() {
    m_playing.store(false, std::memory_order_release);
}

void AudioMixer::stop() {
    m_playing.store(false, std::memory_order_release);
    if (m_sink) m_sink->stop();
}

void AudioMixer::setTrackMute(int trackIdx, bool muted) {
    QMutexLocker lock(&m_controlMutex);
    if (trackIdx < 0 || trackIdx >= kMaxAudioTracks) return;
    if (m_trackStates.size() < trackIdx + 1)
        m_trackStates.resize(trackIdx + 1);
    m_trackStates[trackIdx].muted = muted;
    recomputeEffectiveGains();
}

void AudioMixer::setTrackSolo(int trackIdx, bool solo) {
    QMutexLocker lock(&m_controlMutex);
    if (trackIdx < 0 || trackIdx >= kMaxAudioTracks) return;
    if (m_trackStates.size() < trackIdx + 1)
        m_trackStates.resize(trackIdx + 1);
    m_trackStates[trackIdx].solo = solo;
    recomputeEffectiveGains();
}

void AudioMixer::setTrackGain(int trackIdx, double gain) {
    QMutexLocker lock(&m_controlMutex);
    if (trackIdx < 0 || trackIdx >= kMaxAudioTracks) return;
    if (m_trackStates.size() < trackIdx + 1)
        m_trackStates.resize(trackIdx + 1);
    m_trackStates[trackIdx].gain = qBound(0.0, gain, 4.0);
    recomputeEffectiveGains();
}

void AudioMixer::recomputeEffectiveGains() {
    bool anySolo = false;
    for (const auto &t : m_trackStates) if (t.solo) { anySolo = true; break; }
    for (auto &t : m_trackStates) {
        const bool audible = anySolo ? t.solo : !t.muted;
        t.effectiveGain = audible ? t.gain : 0.0;
    }
}

void AudioMixer::releaseAllEntries() {
    QMutexLocker lock(&m_controlMutex);
    for (auto *e : m_entries) {
        if (e) {
            // US-3 will cleanly close FFmpeg resources here.
            delete e;
        }
    }
    m_entries.clear();
}
