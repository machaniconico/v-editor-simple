#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QHash>
#include <QMutex>
#include <QIODevice>
#include <QAudioSink>
#include <QAudioFormat>
#include <QThread>
#include <atomic>

#include "PlaybackTypes.h"

// AudioMixer — sums up to MAX_AUDIO_TRACKS independent FFmpeg-decoded
// audio streams into a single 48 kHz s16 stereo output via QAudioSink in
// push mode. Replaces the old single-source QMediaPlayer side-player. The
// class header keeps FFmpeg includes private; AudioDecoderEntry and
// MixerIODevice are forward-declared and defined in AudioMixer.cpp.
//
// Thread model:
//   * GUI thread owns AudioMixer and calls setSequence/seekTo/play/pause.
//   * QAudioSink invokes MixerIODevice::readData on its internal worker
//     thread; that path locks m_controlMutex briefly to read the entry
//     list and per-track gains.
//   * The decode thread (m_decodeThread) does FFmpeg packet pull /
//     swresample resampling and pushes samples into each entry's ring
//     buffer. It is added in US-3.
//
// Master clock contract: m_writeCursorUs is the audible timeline-space
// position. The audio callback publishes updates with release semantics;
// VideoPlayer's scheduling code reads with acquire. The clock never
// rewinds except on an explicit seekTo.
struct AudioDecoderEntry;
class MixerIODevice;

struct AudioTrackKey {
    QString filePath;
    qint64 clipInMs = 0;
    int sourceTrack = 0;
    int sourceClipIndex = -1;
    bool operator==(const AudioTrackKey &o) const noexcept {
        return filePath == o.filePath
            && clipInMs == o.clipInMs
            && sourceTrack == o.sourceTrack
            && sourceClipIndex == o.sourceClipIndex;
    }
};
inline uint qHash(const AudioTrackKey &k, uint seed = 0) noexcept {
    return qHash(k.filePath, seed)
         ^ qHash(k.clipInMs, seed)
         ^ qHash(k.sourceTrack, seed)
         ^ qHash(k.sourceClipIndex, seed);
}

class AudioMixer : public QObject {
    Q_OBJECT
public:
    static constexpr int kSampleRateHz = 48000;
    static constexpr int kChannels = 2;
    static constexpr int kBytesPerSample = 2;                  // s16
    static constexpr int kBytesPerFrame = kChannels * kBytesPerSample;
    static constexpr int kMaxAudioTracks = 16;

    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer() override;

    // Replace the active timeline schedule. Opens decoders for new entries
    // and releases decoders for entries no longer present. Safe from GUI
    // thread; locks m_controlMutex briefly.
    void setSequence(const QVector<PlaybackEntry> &entries);

    // Jump the audible playhead. Resyncs FFmpeg seek inside every active
    // entry and flushes ring buffers so the next sample read is the new
    // position.
    void seekTo(int64_t timelineUs);

    // Master clock — audible position in timeline microseconds. Drives
    // VideoPlayer scheduleNextFrame / correctVideoDriftAgainstAudioClock.
    int64_t masterClockUs() const {
        return m_writeCursorUs.load(std::memory_order_acquire);
    }

    // Transport
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return m_playing.load(std::memory_order_acquire); }

    // Per-track controls. trackIdx is sourceTrack from PlaybackEntry.
    void setTrackMute(int trackIdx, bool muted);
    void setTrackSolo(int trackIdx, bool solo);
    void setTrackGain(int trackIdx, double gain);

signals:
    void decoderError(const QString &message);

private:
    friend class MixerIODevice;

    struct TrackState {
        bool muted = false;
        bool solo = false;
        double gain = 1.0;
        double effectiveGain = 1.0;  // gain * mute factor * solo factor
    };

    void recomputeEffectiveGains();
    void releaseAllEntries();

    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    MixerIODevice *m_io = nullptr;

    QHash<AudioTrackKey, AudioDecoderEntry *> m_entries;
    QVector<TrackState> m_trackStates;

    QMutex m_controlMutex;
    std::atomic<int64_t> m_writeCursorUs{0};
    std::atomic<bool> m_playing{false};

    QThread m_decodeThread;  // wired in US-3
};
