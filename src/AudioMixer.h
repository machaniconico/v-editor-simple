#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QHash>
#include <QMutex>
#include <QIODevice>
#include <QAudioSink>
#include <QAudioFormat>
#include <QElapsedTimer>
#include <array>
#include <atomic>

#include "PlaybackTypes.h"
#include "AudioEQ.h"

// AudioMixer — sums up to MAX_AUDIO_TRACKS independent FFmpeg-decoded
// audio streams into a single 48 kHz s16 stereo output via QAudioSink in
// push mode. Replaces the old single-source QMediaPlayer side-player. The
// class header keeps FFmpeg includes private; AudioDecoderEntry,
// MixerIODevice, and AudioDecodeRunner are forward-declared and defined
// in AudioMixer.cpp.
//
// Thread model:
//   * GUI thread owns AudioMixer and calls setSequence/seekTo/play/pause.
//   * QAudioSink invokes MixerIODevice::readData on its internal worker
//     thread; that path locks m_controlMutex briefly to drain ring data
//     and read per-track gains.
//   * AudioDecodeRunner runs on a dedicated QThread, periodically taking
//     m_controlMutex to refill ring buffers ahead of the audio sink.
//
// Master clock contract: m_writeCursorUs is the audible timeline-space
// position. The audio callback publishes updates with release semantics;
// VideoPlayer's scheduling code reads with acquire. The clock never
// rewinds except on an explicit seekTo.
struct AudioDecoderEntry;
class MixerIODevice;
class AudioDecodeRunner;

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
    // Boost-style hash combine. XOR-of-equal-seed collides whenever fields
    // pairwise produce identical hashes (e.g. sourceTrack == sourceClipIndex);
    // with hash_combine the bits diffuse properly.
    uint h = qHash(k.filePath, seed);
    h ^= qHash(k.clipInMs, seed) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= qHash(k.sourceTrack, seed) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= qHash(k.sourceClipIndex, seed) + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

class AudioMixer : public QObject {
    Q_OBJECT
public:
    static constexpr int kSampleRateHz = 48000;
    static constexpr int kChannels = 2;
    static constexpr int kBytesPerSample = 2;                  // s16
    static constexpr int kBytesPerFrame = kChannels * kBytesPerSample;
    static constexpr int kMaxAudioTracks = 16;
    static constexpr int kRingTargetBytes = 64 * 1024;         // ~340 ms stereo s16 @ 48k
    static constexpr int kPrerollLeadUs = 2'000'000;           // pre-warm 2 s before entry start

    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer() override;

    // Replace the active timeline schedule. Opens decoders for new entries
    // and releases decoders for entries no longer present. Safe from GUI
    // thread; locks m_controlMutex briefly.
    void setSequence(const QVector<PlaybackEntry> &entries);

    // Jump the audible playhead. Resyncs FFmpeg seek inside every active
    // entry (lazily on next refill) and flushes ring buffers so the next
    // sample read is at the new position.
    void seekTo(int64_t timelineUs);

    // Master clock — position the mixer has DELIVERED samples to the OS
    // audio buffer through. Note this runs ahead of what the user actually
    // hears by sink->bufferSize() (≈200 ms): the difference is samples sat
    // in the hardware buffer, not yet played. VideoPlayer pace/drift code
    // can use audibleClockUs() to compensate when sub-frame accuracy
    // matters.
    int64_t masterClockUs() const {
        return m_writeCursorUs.load(std::memory_order_acquire);
    }
    int64_t audibleClockUs() const;

    // Transport
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return m_playing.load(std::memory_order_acquire); }

    // Per-track controls. trackIdx is sourceTrack from PlaybackEntry.
    void setTrackMute(int trackIdx, bool muted);
    void setTrackSolo(int trackIdx, bool solo);
    void setTrackGain(int trackIdx, double gain);

    // Per-track realtime EQ (3-band biquad, applied before effectiveGain).
    void setTrackEqConfig(int trackIdx, const AudioEQConfig &cfg);
    AudioEQConfig trackEqConfig(int trackIdx) const;
    void setTrackEqEnabled(int trackIdx, bool enabled);

    // Master-bus compressor + brick-wall limiter, applied to the sum-mixed
    // master output after per-track EQ/gain and the loudness normalizer,
    // before s16 clamping. Disabled by default (bit-exact bypass).
    struct CompressorParams {
        double thresholdDb = -12;
        double ratio = 4;
        double attackMs = 10;
        double releaseMs = 120;
        double makeupDb = 0;
        bool enabled = false;
    };
    void setCompressorParams(const CompressorParams &params);
    void setCompressorEnabled(bool enabled);
    CompressorParams compressorParams() const;
    bool compressorEnabled() const;

    // Master loudness normalizer (FCP-style Loudness effect, applied to the
    // sum-mixed master output before s16 clamping).
    //   amount     0..1 — 0 bypasses entirely, 1 = full target-gain follow.
    //   uniformity 0..1 — 0 = slow smoothing (preserves dynamics),
    //                     1 = fast smoothing (uniform output).
    void setNormalizerAmount(double amount);
    void setNormalizerUniformity(double uniformity);

    // Auto-ducking parameters. Drive the gain reduction applied to BGM
    // tracks when the voice track is active. Wired into Timeline's
    // envelope-based applyDuckingFromTrack (duckGain derived from threshold,
    // attack/release passed as seconds).
    struct AutoDuckParams {
        double thresholdDb = -20.0;
        double ratio = 4.0;
        double attackMs = 5.0;
        double releaseMs = 250.0;
    };
    void setAutoDuckParams(const AutoDuckParams &params);
    AutoDuckParams autoDuckParams() const;

signals:
    void decoderError(const QString &message);
    void levelChanged(int trackIdx, float peakL, float peakR, float rmsL, float rmsR);
    void masterLevelChanged(float pkL, float pkR, float rmsL, float rmsR);

private:
    friend class MixerIODevice;
    friend class AudioDecodeRunner;

    struct EqBandCoefs {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

public:
    struct EqBandCache {
        double frequency = 0;
        double q = 0;
    };

    struct TrackState {
        bool muted = false;
        bool solo = false;
        double gain = 1.0;
        double effectiveGain = 1.0;  // gain * mute factor * solo factor
        bool eqEnabled = false;
        AudioEQConfig eq;
        std::array<EqBandCoefs, 3> eqCoeffs;
        std::array<std::array<double, 4>, 3> z{}; // per-band: z1_L, z1_R, z2_L, z2_R (transposed DF2)
        std::array<EqBandCache, 3> eqCache{};
    };
private:

    bool ensureSinkLocked();              // m_controlMutex must be held
    void recomputeEffectiveGainsLocked(); // m_controlMutex must be held
    void releaseAllEntriesLocked();       // m_controlMutex must be held
    bool openEntry(AudioDecoderEntry *e);
    void closeEntry(AudioDecoderEntry *e);
    void seekEntryToTimeline(AudioDecoderEntry *e, int64_t timelineUs);
    void refillRingForEntry(AudioDecoderEntry *e, int targetBytes);
    void resampleAndAppend(AudioDecoderEntry *e);
    bool refillRings();                   // called by AudioDecodeRunner; returns whether work was done

    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    MixerIODevice *m_io = nullptr;

    QHash<AudioTrackKey, AudioDecoderEntry *> m_entries;
    QVector<TrackState> m_trackStates;

    mutable QMutex m_controlMutex;
    std::atomic<int64_t> m_writeCursorUs{0};
    // Phase 1e Win #13 — Fix K: time-based scrub dedup for seekTo. The
    // existing us-equality + Active + playing early-return (Fix C/F) only
    // skips identical re-seeks. During slider scrub or post-loadFile
    // settling the timeline position changes monotonically every 35-40 ms,
    // so each call wins the dedup but still pays a synchronous QAudioSink
    // stop/start cycle (~15 ms on the main thread). Empirical log
    // veditor_20260501_103732.log @ 10:39:39.673-983 captured 8 such calls
    // in 310 ms, monopolising the GUI thread for ~120 ms and slipping
    // scheduleNextFrame into !advanced auto-pause. The cascade then
    // multiplied via Fix J's 200 ms window (which only catches play()
    // bursts, not seek bursts). Within a 50 ms window we update the cursor,
    // reset the per-entry ring, and wake the decode runner — but skip the
    // sink stop/start. The next seekTo outside the window will run the
    // full path; cursor never lags more than one window.
    QElapsedTimer m_lastSeekToCallTimer;
    // OS-buffered samples in microseconds, published by MixerIODevice::readData
    // so audibleClockUs() is lock-free. Reading m_sink->bytesFree() from the
    // GUI thread under m_controlMutex caused starvation of the audio worker
    // thread (called audibleClockUs every video tick).
    std::atomic<int64_t> m_audibleLagUs{0};
    // Consecutive readData callbacks that wanted to mix an active entry but
    // found its ring empty. Drives the cursor-stall logic so cursor doesn't
    // race past unfilled rings while still self-healing if the decoder is
    // permanently broken.
    std::atomic<int> m_consecutiveStallCallbacks{0};
    std::atomic<bool> m_playing{false};

    // Master compressor state. Params are set from GUI thread under
    // m_controlMutex; readData reads them under the same mutex. The
    // envelope follower state (m_compressorEnv) is touched only from
    // readData on the audio worker thread.
    CompressorParams m_compressorParams;
    double m_compressorEnv = 0.0;

    // Auto-ducking parameters. Set from GUI thread under m_controlMutex;
    // read by ducking menu handler (GUI thread) to drive Timeline's
    // envelope-based applyDuckingFromTrack.
    AutoDuckParams m_autoDuckParams;

    // Master loudness normalizer state. Atomics are touched from the GUI
    // thread (setters) and the audio worker thread (readData). The mutable
    // RMS / smoothed-gain fields are touched only from readData.
    std::atomic<double> m_normalizerAmount{0.0};
    std::atomic<double> m_normalizerUniformity{0.5};
    double m_normalizerRmsMeanSq = 0.0;
    double m_normalizerSmoothedGain = 1.0;

    // Per-track + master level-meter accumulators. readData gathers
    // peak/RMS per fragment and emits levelChanged / masterLevelChanged
    // throttled to <=30 Hz.
    struct LevelAccum {
        float peakL = 0.f;
        float peakR = 0.f;
        double sumSqL = 0.0;
        double sumSqR = 0.0;
        qint64 chanCount = 0;     // per-channel sample count
        void reset() { *this = LevelAccum(); }
    };
    QVector<LevelAccum> m_trackLevelAccum;
    LevelAccum m_masterLevelAccum;
    qint64 m_lastLevelEmitNs = 0;

    AudioDecodeRunner *m_decodeRunner = nullptr;
};
