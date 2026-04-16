#pragma once

#include <QWidget>
#include <QLabel>
#include <QImage>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVector>
#include "VideoEffect.h"
#include "PlaybackTypes.h"

class GLPreview;
class QResizeEvent;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

class VideoPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer();

    void loadFile(const QString &filePath);
    // Replace the active playback schedule with a multi-clip sequence. When
    // non-empty, seek/playback are interpreted in timeline-space and files are
    // switched automatically at clip boundaries. Empty argument falls back to
    // single-file mode (current loaded file is left intact).
    void setSequence(const QVector<PlaybackEntry> &entries);
    void setCanvasSize(int width, int height);
    void setColorCorrection(const ColorCorrection &cc);
    bool isGLAccelerated() const { return m_useGL; }
    void setGLAcceleration(bool enabled);
    GLPreview *glPreview() const { return m_glPreview; }

public slots:
    void play();
    void pause();
    void stop();
    void seek(int positionMs);
    void previewSeek(int positionMs);
    void setPlaybackSpeed(double speed);
    void speedUp();    // L key
    void speedDown();  // J key
    void togglePlay(); // K key

signals:
    void positionChanged(double positionSeconds);
    void durationChanged(double durationSeconds);
    void stateChanged(bool playing);
    void playbackSpeedChanged(double speed);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QSize fittedDisplaySize(const QSize &bounds) const;
    double effectiveDisplayAspectRatio() const;
    double streamDisplayAspectRatio() const;
    void refreshDisplayedFrame();
    void setupUI();
    void resetDecoder();
    void scheduleNextFrame();
    void performPendingSeek();
    void updatePositionUi();
    bool seekInternal(int64_t positionUs, bool displayFrame, bool precise);
    bool decodeNextFrame(bool displayFrame);
    bool presentDecodedFrame(AVFrame *frame, bool displayFrame);
    QImage frameToImage(const AVFrame *frame);
    AVFrame *ensureSwFrame(AVFrame *frame);
    static enum AVPixelFormat getHwFormatCallback(AVCodecContext *ctx, const enum AVPixelFormat *pixFmts);
    int64_t streamFrameDurationUs() const;
    int64_t streamTimestampForPosition(int64_t positionUs) const;
    int64_t positionFromStreamTimestamp(int64_t timestamp) const;
    int sliderPositionForUs(int64_t positionUs) const;
    void handlePlaybackTick();
    void updatePlayButton();
    void displayFrame(const QImage &image);

    // Sequence helpers (Phase A/B). When m_sequence is empty, the player runs
    // in single-file legacy mode and these are unused.
    bool sequenceActive() const { return !m_sequence.isEmpty(); }
    int findActiveEntryAt(int64_t timelineUs) const;
    int64_t entryLocalPositionUs(int entryIdx, int64_t timelineUs) const;
    int64_t fileLocalToTimelineUs(int entryIdx, int64_t fileLocalUs) const;
    bool seekToTimelineUs(int64_t timelineUs, bool precise);
    bool advanceToEntry(int newEntryIdx);
    void applySequenceSliderRange();
    int sliderTimelinePosition(int64_t timelineUs) const;
    void applyActiveEntryAudioMute();

    QLabel *m_videoDisplay;
    GLPreview *m_glPreview = nullptr;
    bool m_useGL = true;
    QPushButton *m_playButton;
    QPushButton *m_pauseButton;
    QPushButton *m_stopButton;
    QSlider *m_seekBar;
    QLabel *m_timeLabel;
    QTimer *m_playbackTimer = nullptr;
    QTimer *m_seekTimer = nullptr;
    QMediaPlayer *m_audioPlayer = nullptr;
    QAudioOutput *m_audioOut = nullptr;

    AVFormatContext *m_formatCtx = nullptr;
    AVCodecContext *m_codecCtx = nullptr;
    SwsContext *m_swsCtx = nullptr;
    AVFrame *m_frame = nullptr;
    AVFrame *m_swFrame = nullptr;
    AVPacket *m_packet = nullptr;
    AVBufferRef *m_hwDeviceCtx = nullptr;
    AVPixelFormat m_hwPixFmt = AV_PIX_FMT_NONE;
    int m_videoStreamIndex = -1;
    bool m_playing = false;
    int64_t m_durationUs = 0;
    int64_t m_currentPositionUs = 0;
    int64_t m_frameDurationUs = 0;
    double m_displayAspectRatio = 0.0;
    int m_canvasWidth = 1920;
    int m_canvasHeight = 1080;
    double m_playbackSpeed = 1.0;
    QImage m_currentFrameImage;
    int m_pendingSeekMs = -1;
    bool m_pendingSeekPrecise = false;
    bool m_seekInProgress = false;
    bool m_suppressUiUpdates = false; // guard against intermediate slider flashes during cross-file seeks
    int m_lastDragMs = -1;
    int m_audioResyncTickCount = 0;
    qint64 m_audioUnmuteToken = 0;
    bool m_audioUnmuteScheduled = false;

    // Multi-clip sequence state. m_currentPositionUs remains FILE-LOCAL (so
    // the existing decoder loop is untouched); m_timelinePositionUs tracks the
    // resolved timeline position when sequence mode is active.
    QVector<PlaybackEntry> m_sequence;
    int m_activeEntry = -1;
    int64_t m_timelinePositionUs = 0;
    int64_t m_sequenceDurationUs = 0;
    QString m_loadedFilePath;
};
