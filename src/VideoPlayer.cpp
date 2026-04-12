#include "VideoPlayer.h"
#include "GLPreview.h"
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QUrl>
#include <QtGlobal>
#include <QDebug>
#include <QPointer>
#include <QTimer>
#include <cmath>
#include <limits>

namespace {

QString formatTimestamp(int64_t positionUs)
{
    const int totalSeconds = static_cast<int>(qMax<int64_t>(0, positionUs / AV_TIME_BASE));
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

}

VideoPlayer::VideoPlayer(QWidget *parent)
    : QWidget(parent)
{
    qInfo() << "VideoPlayer::ctor";
    setupUI();
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setSingleShot(true);
    connect(m_playbackTimer, &QTimer::timeout, this, &VideoPlayer::handlePlaybackTick);

    m_seekTimer = new QTimer(this);
    m_seekTimer->setSingleShot(true);
    m_seekTimer->setInterval(15);
    connect(m_seekTimer, &QTimer::timeout, this, &VideoPlayer::performPendingSeek);

    m_audioPlayer = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_audioOut->setVolume(1.0);
    m_audioPlayer->setAudioOutput(m_audioOut);
    connect(m_audioPlayer, &QMediaPlayer::errorOccurred, this,
            [](QMediaPlayer::Error err, const QString &msg) {
                if (err != QMediaPlayer::NoError)
                    qWarning() << "QMediaPlayer error:" << err << msg;
            });
}

VideoPlayer::~VideoPlayer()
{
    qInfo() << "VideoPlayer::dtor";
    // Stop any in-flight timers before decoder teardown so a queued
    // handlePlaybackTick doesn't fire on a half-destroyed object.
    if (m_playbackTimer) m_playbackTimer->stop();
    if (m_seekTimer)     m_seekTimer->stop();
    resetDecoder();
}

void VideoPlayer::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    auto *displayStack = new QStackedWidget(this);

    m_videoDisplay = new QLabel(this);
    m_videoDisplay->setAlignment(Qt::AlignCenter);
    m_videoDisplay->setMinimumSize(800, 450);
    m_videoDisplay->setText("Drop a video file or use File > Open");
    m_videoDisplay->setStyleSheet("background-color: #1a1a1a; color: #888; font-size: 16px;");

    m_glPreview = new GLPreview(this);
    m_glPreview->setMinimumSize(640, 360);

    displayStack->addWidget(m_videoDisplay); // index 0: software
    displayStack->addWidget(m_glPreview);    // index 1: GL
    displayStack->setCurrentIndex(m_useGL ? 1 : 0);

    layout->addWidget(displayStack, 1);

    auto *controls = new QHBoxLayout();

    // Unicode media controls: ▶ U+25B6 / ⏸ U+23F8 / ⏹ U+23F9
    m_playButton = new QPushButton(QString::fromUtf8("\xE2\x96\xB6"), this);
    m_pauseButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xB8"), this);
    m_stopButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xB9"), this);
    m_seekBar = new QSlider(Qt::Horizontal, this);
    m_timeLabel = new QLabel("00:00 / 00:00", this);

    const QString mediaBtnStyle =
        "QPushButton { background-color: #444; color: #ddd; border: 1px solid #666;"
        "  border-radius: 4px; font-size: 16px; padding: 0; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:pressed { background-color: #666; }"
        "QPushButton:disabled { color: #777; background-color: #383838; }";
    m_playButton->setFixedSize(40, 32);
    m_pauseButton->setFixedSize(40, 32);
    m_stopButton->setFixedSize(40, 32);
    m_playButton->setStyleSheet(mediaBtnStyle);
    m_pauseButton->setStyleSheet(mediaBtnStyle);
    m_stopButton->setStyleSheet(mediaBtnStyle);
    m_playButton->setToolTip(QStringLiteral("再生"));
    m_pauseButton->setToolTip(QStringLiteral("一時停止"));
    m_stopButton->setToolTip(QStringLiteral("停止"));
    m_timeLabel->setFixedWidth(120);
    m_seekBar->setRange(0, 0);
    m_seekBar->setTracking(false);

    controls->addWidget(m_playButton);
    controls->addWidget(m_pauseButton);
    controls->addWidget(m_stopButton);
    controls->addWidget(m_seekBar);
    controls->addWidget(m_timeLabel);

    layout->addLayout(controls);

    connect(m_playButton, &QPushButton::clicked, this, &VideoPlayer::play);
    connect(m_pauseButton, &QPushButton::clicked, this, &VideoPlayer::pause);
    connect(m_stopButton, &QPushButton::clicked, this, &VideoPlayer::stop);
    connect(m_seekBar, &QSlider::sliderMoved, this, [this](int pos) {
        m_lastDragMs = pos;
        previewSeek(pos);
    });
    connect(m_seekBar, &QSlider::sliderReleased, this, [this]() {
        if (m_lastDragMs >= 0)
            seek(m_lastDragMs);
    });
    connect(m_seekBar, &QSlider::valueChanged, this, [this](int value) {
        if (m_lastDragMs >= 0) {
            m_lastDragMs = -1;
            return;
        }
        if (!m_seekBar->isSliderDown())
            seek(value);
    });
}

void VideoPlayer::loadFile(const QString &filePath)
{
    qInfo() << "VideoPlayer::loadFile BEGIN" << filePath;
    // Mute the side-player BEFORE we tear down so any tail samples queued in
    // QAudioOutput don't overlap with the next file's start (root cause of
    // the "double playback" report). Scheduling a 200ms-delayed unmute makes
    // sure Qt's Media Foundation backend has fully drained the previous
    // source AND has decoded the first samples of the new source before we
    // raise the gate again. Token guards against newer loadFile calls
    // beating the timer.
    if (m_audioOut)
        m_audioOut->setMuted(true);
    m_audioUnmuteScheduled = true;
    ++m_audioUnmuteToken;
    const qint64 myToken = m_audioUnmuteToken;
    QPointer<VideoPlayer> guard(this);
    QTimer::singleShot(200, this, [guard, myToken]() {
        if (!guard) return;
        if (myToken != guard->m_audioUnmuteToken) return; // superseded
        guard->m_audioUnmuteScheduled = false;
        guard->applyActiveEntryAudioMute();
    });

    resetDecoder();
    qInfo() << "  resetDecoder done";
    m_loadedFilePath = filePath;

    const QByteArray pathUtf8 = filePath.toUtf8();
    if (avformat_open_input(&m_formatCtx, pathUtf8.constData(), nullptr, nullptr) != 0) {
        qWarning() << "avformat_open_input failed for" << filePath;
        m_videoDisplay->setText("Failed to open file");
        return;
    }
    qInfo() << "  avformat_open_input ok";

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        qWarning() << "avformat_find_stream_info failed";
        resetDecoder();
        m_videoDisplay->setText("Failed to read stream info");
        return;
    }
    qInfo() << "  avformat_find_stream_info ok, nb_streams=" << m_formatCtx->nb_streams;

    m_videoStreamIndex = -1;
    for (unsigned i = 0; i < m_formatCtx->nb_streams; i++) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (m_videoStreamIndex < 0) {
        resetDecoder();
        m_videoDisplay->setText("No video stream found");
        return;
    }

    auto *codecpar = m_formatCtx->streams[m_videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        resetDecoder();
        m_videoDisplay->setText("Unsupported codec");
        return;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx || avcodec_parameters_to_context(m_codecCtx, codecpar) < 0) {
        resetDecoder();
        m_videoDisplay->setText("Failed to initialize codec");
        return;
    }

    m_hwPixFmt = AV_PIX_FMT_NONE;
    if (av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0) >= 0) {
        for (int i = 0;; i++) {
            const AVCodecHWConfig *cfg = avcodec_get_hw_config(codec, i);
            if (!cfg)
                break;
            if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                cfg->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
                m_hwPixFmt = cfg->pix_fmt;
                break;
            }
        }
    }

    if (m_hwPixFmt != AV_PIX_FMT_NONE && m_hwDeviceCtx) {
        m_codecCtx->opaque = this;
        m_codecCtx->get_format = &VideoPlayer::getHwFormatCallback;
        m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
        qInfo() << "  HW decode enabled via D3D11VA";
    } else {
        if (m_hwDeviceCtx) {
            av_buffer_unref(&m_hwDeviceCtx);
            m_hwDeviceCtx = nullptr;
        }
        qInfo() << "  HW decode unavailable, using software decoding";
    }

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        resetDecoder();
        m_videoDisplay->setText("Failed to open codec");
        return;
    }

    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    m_swFrame = av_frame_alloc();
    if (!m_packet || !m_frame || !m_swFrame) {
        resetDecoder();
        m_videoDisplay->setText("Failed to allocate decode buffers");
        return;
    }
    qInfo() << "  packet/frame allocated";

    if (m_formatCtx->duration > 0) {
        m_durationUs = m_formatCtx->duration;
    } else {
        AVStream *stream = m_formatCtx->streams[m_videoStreamIndex];
        if (stream->duration > 0)
            m_durationUs = av_rescale_q(stream->duration, stream->time_base, AV_TIME_BASE_Q);
    }

    m_frameDurationUs = streamFrameDurationUs();
    m_displayAspectRatio = streamDisplayAspectRatio();
    qInfo() << "  duration=" << m_durationUs << "frameDur=" << m_frameDurationUs
            << "aspect=" << m_displayAspectRatio;
    if (m_glPreview)
        m_glPreview->setDisplayAspectRatio(m_displayAspectRatio);
    if (!m_suppressUiUpdates) {
        m_seekBar->setRange(0, sliderPositionForUs(m_durationUs));
        emit durationChanged(static_cast<double>(m_durationUs) / AV_TIME_BASE);
    }

    qInfo() << "  entering seekInternal(0)";
    if (!seekInternal(0, true, true)) {
        qWarning() << "seekInternal(0) failed";
        resetDecoder();
        m_videoDisplay->setText("Failed to decode first frame");
        return;
    }
    qInfo() << "  seekInternal(0) ok";

    if (m_audioPlayer) {
        m_audioPlayer->stop();
        m_audioPlayer->setSource(QUrl::fromLocalFile(filePath));
        m_audioPlayer->setPosition(0);
        if (m_audioOut)
            m_audioOut->setMuted(false);
    }

    // If a sequence is active, restore its slider range so the seekbar shows
    // the full timeline rather than just this file's duration.
    if (sequenceActive()) {
        applySequenceSliderRange();
        emit durationChanged(static_cast<double>(m_sequenceDurationUs) / AV_TIME_BASE);
    }

    updatePositionUi();
    qInfo() << "VideoPlayer::loadFile END";
}

void VideoPlayer::setSequence(const QVector<PlaybackEntry> &entries)
{
    qInfo() << "VideoPlayer::setSequence count=" << entries.size();

    // Compute total duration in microseconds.
    int64_t totalUs = 0;
    for (const auto &e : entries) {
        const int64_t entryEndUs = static_cast<int64_t>(e.timelineEnd * AV_TIME_BASE);
        if (entryEndUs > totalUs) totalUs = entryEndUs;
    }

    m_sequence = entries;
    m_sequenceDurationUs = totalUs;

    if (entries.isEmpty()) {
        // Timeline emptied (all clips deleted). Pause and clear the slider so
        // the player doesn't keep ticking against a stale file. We don't tear
        // down the decoder — preview path may still be using it.
        m_activeEntry = -1;
        m_timelinePositionUs = 0;
        if (m_playing) pause();
        m_seekBar->setRange(0, 0);
        emit durationChanged(0.0);
        return;
    }

    // Slider range follows the sequence total.
    applySequenceSliderRange();
    emit durationChanged(static_cast<double>(m_sequenceDurationUs) / AV_TIME_BASE);

    // Clamp current timeline position into the new sequence and pick an
    // active entry. Default to 0 if we have no prior position (e.g. first add).
    int64_t clamped = qBound<int64_t>(0, m_timelinePositionUs, m_sequenceDurationUs);
    int desiredIdx = findActiveEntryAt(clamped);
    if (desiredIdx < 0) {
        desiredIdx = 0;
        clamped = static_cast<int64_t>(m_sequence.first().timelineStart * AV_TIME_BASE);
    }

    const auto &target = m_sequence[desiredIdx];
    const bool needFileSwitch = (target.filePath != m_loadedFilePath) || !m_formatCtx;
    // Idempotency: if the structurally identical entry is already active and
    // the file is already loaded, skip the seek/setSource entirely so back-to-
    // back sequenceChanged emissions (e.g. video track + audio track both
    // emitting modified() during a single addClip) don't repeatedly disturb
    // the QMediaPlayer side-player and cause audible double playback.
    const bool entryStructurallyChanged = needFileSwitch || (m_activeEntry != desiredIdx);

    if (needFileSwitch) {
        const bool wasPlaying = m_playing;
        loadFile(target.filePath);  // resets decoder, clears m_playing
        if (wasPlaying)
            m_playing = true;
    }

    m_activeEntry = desiredIdx;
    m_timelinePositionUs = clamped;

    if (entryStructurallyChanged) {
        const int64_t localUs = entryLocalPositionUs(desiredIdx, clamped);
        seekInternal(localUs, true, true);

        // loadFile() already called m_audioPlayer->setSource() when
        // needFileSwitch was true; calling setSource a second time here would
        // spawn a parallel decoder in Qt's Media Foundation backend and the
        // user hears the same audio playing twice. Only adjust the position.
        if (m_audioPlayer)
            m_audioPlayer->setPosition(qMax<qint64>(0, localUs / 1000));
    }
    // Audio mute may have changed even when the entry is otherwise unchanged
    // (e.g. user toggled the M button on the active track) — always reapply.
    applyActiveEntryAudioMute();

    updatePositionUi();
}

int VideoPlayer::findActiveEntryAt(int64_t timelineUs) const
{
    if (m_sequence.isEmpty()) return -1;
    const double tSec = static_cast<double>(timelineUs) / AV_TIME_BASE;
    for (int i = 0; i < m_sequence.size(); ++i) {
        const auto &e = m_sequence[i];
        if (tSec >= e.timelineStart && tSec < e.timelineEnd)
            return i;
    }
    // At or past the very end → last entry.
    if (tSec >= m_sequence.last().timelineEnd - 1e-6)
        return m_sequence.size() - 1;
    // Before the first → first.
    if (tSec < m_sequence.first().timelineStart)
        return 0;
    return -1;
}

int64_t VideoPlayer::entryLocalPositionUs(int entryIdx, int64_t timelineUs) const
{
    if (entryIdx < 0 || entryIdx >= m_sequence.size()) return 0;
    const auto &e = m_sequence[entryIdx];
    const double tSec = static_cast<double>(timelineUs) / AV_TIME_BASE;
    const double offsetIntoEntry = qMax(0.0, tSec - e.timelineStart);
    const double speed = (e.speed > 0.0) ? e.speed : 1.0;
    const double localSec = e.clipIn + offsetIntoEntry * speed;
    return static_cast<int64_t>(localSec * AV_TIME_BASE);
}

int64_t VideoPlayer::fileLocalToTimelineUs(int entryIdx, int64_t fileLocalUs) const
{
    if (entryIdx < 0 || entryIdx >= m_sequence.size()) return 0;
    const auto &e = m_sequence[entryIdx];
    const double fileLocalSec = static_cast<double>(fileLocalUs) / AV_TIME_BASE;
    const double speed = (e.speed > 0.0) ? e.speed : 1.0;
    const double offsetIntoEntry = qMax(0.0, (fileLocalSec - e.clipIn) / speed);
    const double timelineSec = e.timelineStart + offsetIntoEntry;
    return static_cast<int64_t>(timelineSec * AV_TIME_BASE);
}

void VideoPlayer::applySequenceSliderRange()
{
    const int64_t sliderMaxMs = qMin<int64_t>(m_sequenceDurationUs / 1000,
                                              std::numeric_limits<int>::max());
    m_seekBar->setRange(0, static_cast<int>(sliderMaxMs));
}

int VideoPlayer::sliderTimelinePosition(int64_t timelineUs) const
{
    const int64_t ms = qMax<int64_t>(0, timelineUs / 1000);
    return static_cast<int>(qMin<int64_t>(ms, std::numeric_limits<int>::max()));
}

void VideoPlayer::applyActiveEntryAudioMute()
{
    if (!m_audioOut) return;
    // Honour the loadFile lockout — the delayed unmute timer will reapply
    // the correct state once the file swap is fully settled.
    if (m_audioUnmuteScheduled) return;
    bool muted = false;
    if (sequenceActive() && m_activeEntry >= 0 && m_activeEntry < m_sequence.size()) {
        muted = m_sequence[m_activeEntry].audioMuted;
    }
    // Reverse playback already mutes audio elsewhere; don't fight that here.
    if (m_playbackSpeed < 0.0)
        muted = true;
    m_audioOut->setMuted(muted);
}

bool VideoPlayer::seekToTimelineUs(int64_t timelineUs, bool precise)
{
    if (m_sequence.isEmpty()) return false;
    timelineUs = qBound<int64_t>(0, timelineUs, m_sequenceDurationUs);
    int idx = findActiveEntryAt(timelineUs);
    if (idx < 0) idx = 0;
    if (idx >= m_sequence.size()) return false;

    const auto &e = m_sequence[idx];
    const bool needSwitch = (e.filePath != m_loadedFilePath) || !m_formatCtx;
    // Freeze UI updates across the loadFile → seek chain so the slider
    // doesn't flash back to 0 while the intermediate seekInternal(0) runs
    // inside loadFile. We explicitly call updatePositionUi at the end.
    const bool prevSuppress = m_suppressUiUpdates;
    m_suppressUiUpdates = needSwitch;
    if (needSwitch) {
        const bool wasPlaying = m_playing;
        loadFile(e.filePath); // sets m_audioPlayer source as part of init
        if (wasPlaying)
            m_playing = true;
    }

    m_activeEntry = idx;
    m_timelinePositionUs = timelineUs;
    const int64_t localUs = entryLocalPositionUs(idx, timelineUs);
    const bool ok = seekInternal(localUs, true, precise);
    applyActiveEntryAudioMute();
    m_suppressUiUpdates = prevSuppress;
    updatePositionUi();
    return ok;
}

bool VideoPlayer::advanceToEntry(int newEntryIdx)
{
    if (newEntryIdx < 0 || newEntryIdx >= m_sequence.size())
        return false;

    const auto &next = m_sequence[newEntryIdx];
    qInfo() << "VideoPlayer::advanceToEntry idx=" << newEntryIdx
            << "file=" << next.filePath
            << "timelineStart=" << next.timelineStart
            << "clipIn=" << next.clipIn;

    const bool wasPlaying = m_playing;
    const bool needSwitch = (next.filePath != m_loadedFilePath);
    // Suppress intermediate UI updates across the loadFile → seek chain so the
    // slider doesn't flash back to 0 while resetDecoder/seekInternal(0) inside
    // loadFile temporarily clear the slider range.
    const bool prevSuppress = m_suppressUiUpdates;
    m_suppressUiUpdates = needSwitch;
    if (needSwitch) {
        loadFile(next.filePath); // loadFile sets the audio source itself
    }

    m_activeEntry = newEntryIdx;
    m_timelinePositionUs = static_cast<int64_t>(next.timelineStart * AV_TIME_BASE);
    const int64_t startLocalUs = static_cast<int64_t>(next.clipIn * AV_TIME_BASE);
    if (!seekInternal(startLocalUs, true, true)) {
        m_suppressUiUpdates = prevSuppress;
        return false;
    }

    if (m_audioPlayer) {
        m_audioPlayer->setPosition(qMax<qint64>(0, startLocalUs / 1000));
        if (wasPlaying && m_playbackSpeed >= 0.0)
            m_audioPlayer->play();
    }
    applyActiveEntryAudioMute();

    if (wasPlaying)
        m_playing = true;
    m_suppressUiUpdates = prevSuppress;
    updatePositionUi();
    return true;
}

void VideoPlayer::setCanvasSize(int width, int height)
{
    m_canvasWidth = width;
    m_canvasHeight = height;
    double ar = static_cast<double>(width) / height;
    m_videoDisplay->setMinimumSize(
        qMin(640, width / 2),
        qMin(360, height / 2));
    if (m_currentFrameImage.isNull()) {
        QString orientation = (ar > 1.0) ? "Landscape" : (ar < 1.0) ? "Portrait" : "Square";
        m_videoDisplay->setText(QString("%1x%2 %3\nDrop a video file or use File > Open")
            .arg(width).arg(height).arg(orientation));
    } else {
        refreshDisplayedFrame();
    }
}

void VideoPlayer::play()
{
    if (!m_formatCtx || !m_codecCtx)
        return;

    if (m_playing)
        return;

    m_playing = true;
    updatePlayButton();
    emit stateChanged(true);
    scheduleNextFrame();

    if (m_audioPlayer && m_audioPlayer->source().isValid() && m_playbackSpeed >= 0.0) {
        if (m_audioOut)
            m_audioOut->setMuted(false);
        m_audioPlayer->setPosition(qMax<int64_t>(0, m_currentPositionUs / 1000));
        m_audioPlayer->play();
    }
}

void VideoPlayer::pause()
{
    if (m_playbackTimer)
        m_playbackTimer->stop();

    m_playing = false;
    updatePlayButton();
    emit stateChanged(false);

    if (m_audioPlayer)
        m_audioPlayer->pause();
}

void VideoPlayer::stop()
{
    pause();
    seekInternal(0, true, true);
    if (m_audioPlayer) {
        m_audioPlayer->stop();
        m_audioPlayer->setPosition(0);
    }
}

void VideoPlayer::seek(int positionMs)
{
    // In sequence mode positionMs is interpreted as TIMELINE ms (the slider
    // and Timeline both speak timeline coordinates). In legacy single-file
    // mode it's the file-local ms — performPendingSeek picks the right path.
    if (!sequenceActive() && (!m_formatCtx || !m_codecCtx))
        return;

    m_pendingSeekMs = qMax(0, positionMs);
    m_pendingSeekPrecise = true;
    if (m_playbackTimer)
        m_playbackTimer->stop();

    if (!m_seekInProgress && m_seekTimer && !m_seekTimer->isActive())
        m_seekTimer->start();
}

void VideoPlayer::previewSeek(int positionMs)
{
    if (!sequenceActive() && (!m_formatCtx || !m_codecCtx))
        return;

    m_pendingSeekMs = qMax(0, positionMs);
    if (m_playbackTimer)
        m_playbackTimer->stop();

    if (!m_seekInProgress && m_seekTimer && !m_seekTimer->isActive())
        m_seekTimer->start();
}

void VideoPlayer::setPlaybackSpeed(double speed)
{
    if (qFuzzyIsNull(speed))
        speed = 1.0;

    const double absSpeed = qBound(0.25, std::abs(speed), 16.0);
    m_playbackSpeed = (speed < 0.0) ? -absSpeed : absSpeed;
    emit playbackSpeedChanged(m_playbackSpeed);

    if (m_playing)
        scheduleNextFrame();

    if (m_audioPlayer) {
        if (m_playbackSpeed < 0.0) {
            if (m_audioOut)
                m_audioOut->setMuted(true);
            m_audioPlayer->pause();
        } else {
            if (m_audioOut)
                m_audioOut->setMuted(false);
            m_audioPlayer->setPlaybackRate(absSpeed);
            if (m_playing && m_audioPlayer->source().isValid())
                m_audioPlayer->play();
        }
    }
}

void VideoPlayer::speedUp()
{
    if (!m_playing) {
        setPlaybackSpeed(1.0);
        play();
        return;
    }

    if (m_playbackSpeed < 0.0)
        setPlaybackSpeed(1.0);
    else
        setPlaybackSpeed(qMin(16.0, m_playbackSpeed * 2.0));
}

void VideoPlayer::speedDown()
{
    if (!m_playing) {
        setPlaybackSpeed(-1.0);
        play();
        return;
    }

    if (m_playbackSpeed > 0.0)
        setPlaybackSpeed(-1.0);
    else
        setPlaybackSpeed(qMax(-16.0, m_playbackSpeed * 2.0));
}

void VideoPlayer::togglePlay()
{
    if (m_playing) {
        pause();
    } else {
        setPlaybackSpeed(1.0);
        play();
    }
}

void VideoPlayer::updatePlayButton()
{
    // With separate Play / Pause / Stop buttons, the icons no longer toggle.
    // Instead enable/disable the buttons that don't apply to the current state
    // so the user gets a visual hint about what's actionable.
    if (m_playButton)  m_playButton->setEnabled(!m_playing);
    if (m_pauseButton) m_pauseButton->setEnabled(m_playing);
    if (m_stopButton)  m_stopButton->setEnabled(true);
}

void VideoPlayer::displayFrame(const QImage &image)
{
    m_currentFrameImage = image;
    if (m_useGL && m_glPreview) {
        m_glPreview->setDisplayAspectRatio(effectiveDisplayAspectRatio());
        m_glPreview->displayFrame(image);
    } else {
        const QSize targetSize = fittedDisplaySize(m_videoDisplay->size());
        const QPixmap pixmap = QPixmap::fromImage(image);
        m_videoDisplay->setPixmap(pixmap.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
}

void VideoPlayer::setColorCorrection(const ColorCorrection &cc)
{
    if (m_glPreview)
        m_glPreview->setColorCorrection(cc);
}

void VideoPlayer::setGLAcceleration(bool enabled)
{
    m_useGL = enabled;
    auto *stack = qobject_cast<QStackedWidget*>(m_videoDisplay->parentWidget());
    if (stack)
        stack->setCurrentIndex(enabled ? 1 : 0);
    refreshDisplayedFrame();
}

void VideoPlayer::resetDecoder()
{
    if (m_playbackTimer)
        m_playbackTimer->stop();
    if (m_seekTimer)
        m_seekTimer->stop();

    if (m_audioPlayer) {
        m_audioPlayer->stop();
        m_audioPlayer->setSource(QUrl());
    }

    m_playing = false;
    m_videoStreamIndex = -1;
    m_durationUs = 0;
    m_currentPositionUs = 0;
    m_frameDurationUs = 0;
    m_displayAspectRatio = 0.0;
    m_currentFrameImage = QImage();
    m_pendingSeekMs = -1;
    m_pendingSeekPrecise = false;
    m_seekInProgress = false;

    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_frame)
        av_frame_free(&m_frame);
    if (m_swFrame)
        av_frame_free(&m_swFrame);
    if (m_packet)
        av_packet_free(&m_packet);
    if (m_codecCtx)
        avcodec_free_context(&m_codecCtx);
    if (m_formatCtx)
        avformat_close_input(&m_formatCtx);
    if (m_hwDeviceCtx)
        av_buffer_unref(&m_hwDeviceCtx);
    m_hwPixFmt = AV_PIX_FMT_NONE;

    updatePlayButton();
    if (!m_suppressUiUpdates)
        m_seekBar->setRange(0, 0);
    if (m_glPreview)
        m_glPreview->setDisplayAspectRatio(0.0);
    updatePositionUi();
}

void VideoPlayer::scheduleNextFrame()
{
    if (!m_playing || !m_playbackTimer)
        return;

    const double absSpeed = qMax(0.25, std::abs(m_playbackSpeed));
    const int64_t baseFrameUs = (m_frameDurationUs > 0) ? m_frameDurationUs : (AV_TIME_BASE / 30);
    const int intervalMs = qMax(5, static_cast<int>(baseFrameUs / 1000.0 / absSpeed));
    m_playbackTimer->start(intervalMs);
}

void VideoPlayer::performPendingSeek()
{
    if (m_seekInProgress || m_pendingSeekMs < 0)
        return;

    const bool seqMode = sequenceActive();
    if (!seqMode && (!m_formatCtx || !m_codecCtx)) {
        m_pendingSeekMs = -1;
        return;
    }

    m_seekInProgress = true;

    const bool wasPlaying = m_playing;
    const int requestedMs = m_pendingSeekMs;
    const bool precise = m_pendingSeekPrecise;
    m_pendingSeekMs = -1;
    m_pendingSeekPrecise = false;

    bool seekOk;
    if (seqMode) {
        const int64_t timelineUs = static_cast<int64_t>(requestedMs) * 1000;
        // Preview seeks (non-precise / drag) MUST NOT switch files. Each
        // file switch tears down the QMediaPlayer side-player and the user
        // hears the old audio briefly overlapping with the new — perceived
        // as "double playback". Refuse cross-file preview seeks; the final
        // committed seek (sliderReleased / valueChanged) handles the switch.
        if (!precise) {
            const int64_t clampedTimelineUs = qBound<int64_t>(0, timelineUs, m_sequenceDurationUs);
            const int idx = findActiveEntryAt(clampedTimelineUs);
            if (idx >= 0 && idx < m_sequence.size()
                && m_sequence[idx].filePath != m_loadedFilePath
                && m_formatCtx) {
                qInfo() << "VideoPlayer: skipping preview file switch idx=" << idx;
                seekOk = false;
            } else {
                seekOk = seekToTimelineUs(timelineUs, precise);
            }
        } else {
            seekOk = seekToTimelineUs(timelineUs, precise);
        }
    } else {
        const int64_t targetUs = static_cast<int64_t>(requestedMs) * 1000;
        seekOk = seekInternal(targetUs, true, precise);
    }

    m_seekInProgress = false;

    if (m_pendingSeekMs >= 0) {
        if (m_seekTimer)
            m_seekTimer->start();
        return;
    }

    if (seekOk && m_audioPlayer && m_audioPlayer->source().isValid()) {
        const qint64 posMs = qMax<int64_t>(0, m_currentPositionUs / 1000);
        m_audioPlayer->setPosition(posMs);
    }

    if (seekOk && wasPlaying)
        scheduleNextFrame();
}

void VideoPlayer::updatePositionUi()
{
    if (m_suppressUiUpdates) return;
    int64_t displayUs;
    int64_t totalUs;
    if (sequenceActive()) {
        // Reproject the current file-local position into timeline coordinates
        // so the slider, time label and positionChanged signal all speak the
        // same timeline-space the Timeline widget uses.
        if (m_activeEntry >= 0 && m_activeEntry < m_sequence.size())
            m_timelinePositionUs = fileLocalToTimelineUs(m_activeEntry, m_currentPositionUs);
        displayUs = m_timelinePositionUs;
        totalUs = m_sequenceDurationUs;
    } else {
        displayUs = m_currentPositionUs;
        totalUs = m_durationUs;
    }

    const int sliderValue = qMin(sliderTimelinePosition(displayUs), m_seekBar->maximum());
    const QSignalBlocker blocker(m_seekBar);
    if (!m_seekBar->isSliderDown())
        m_seekBar->setValue(sliderValue);
    m_timeLabel->setText(QString("%1 / %2")
        .arg(formatTimestamp(displayUs))
        .arg(formatTimestamp(totalUs)));
    emit positionChanged(static_cast<double>(displayUs) / AV_TIME_BASE);
}

bool VideoPlayer::seekInternal(int64_t positionUs, bool displayFrame, bool precise)
{
    if (!m_formatCtx || !m_codecCtx || m_videoStreamIndex < 0)
        return false;

    AVStream *stream = m_formatCtx->streams[m_videoStreamIndex];
    const int64_t targetUs = qMax<int64_t>(0, (m_durationUs > 0) ? qMin(positionUs, m_durationUs) : positionUs);
    const int64_t targetTimestamp = streamTimestampForPosition(targetUs);

    if (av_seek_frame(m_formatCtx, m_videoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD) < 0)
        return false;

    avcodec_flush_buffers(m_codecCtx);
    m_currentPositionUs = targetUs;

    if (!displayFrame) {
        updatePositionUi();
        return true;
    }

    bool foundFrame = false;
    while (decodeNextFrame(false)) {
        foundFrame = true;
        if (!precise || m_currentPositionUs >= targetUs)
            break;
    }

    if (foundFrame && m_frame) {
        AVFrame *displayable = ensureSwFrame(m_frame);
        if (!displayable)
            return false;
        const QImage image = frameToImage(displayable);
        if (image.isNull())
            return false;
        this->displayFrame(image);
        m_currentPositionUs = targetUs;
        updatePositionUi();
        return true;
    }

    updatePositionUi();
    return false;
}

bool VideoPlayer::decodeNextFrame(bool displayFrame)
{
    if (!m_formatCtx || !m_codecCtx || !m_packet || !m_frame)
        return false;

    const auto receiveFrame = [this, displayFrame]() -> bool {
        const int receiveResult = avcodec_receive_frame(m_codecCtx, m_frame);
        if (receiveResult == 0)
            return presentDecodedFrame(m_frame, displayFrame);
        return false;
    };

    if (receiveFrame())
        return true;

    while (av_read_frame(m_formatCtx, m_packet) >= 0) {
        if (m_packet->stream_index != m_videoStreamIndex) {
            av_packet_unref(m_packet);
            continue;
        }

        const int sendResult = avcodec_send_packet(m_codecCtx, m_packet);
        av_packet_unref(m_packet);
        if (sendResult < 0)
            continue;

        if (receiveFrame())
            return true;
    }

    if (avcodec_send_packet(m_codecCtx, nullptr) >= 0 && receiveFrame())
        return true;

    return false;
}

bool VideoPlayer::presentDecodedFrame(AVFrame *frame, bool displayFrameRequested)
{
    int64_t positionUs = m_currentPositionUs;
    const int64_t bestEffortTimestamp =
        (frame->best_effort_timestamp != AV_NOPTS_VALUE) ? frame->best_effort_timestamp : frame->pts;

    if (bestEffortTimestamp != AV_NOPTS_VALUE) {
        positionUs = positionFromStreamTimestamp(bestEffortTimestamp);
    } else if (m_frameDurationUs > 0) {
        positionUs += m_frameDurationUs;
    }

    positionUs = qMax<int64_t>(0, positionUs);
    if (m_durationUs > 0)
        positionUs = qMin(positionUs, m_durationUs);
    m_currentPositionUs = positionUs;

    if (displayFrameRequested) {
        AVFrame *displayable = ensureSwFrame(frame);
        if (!displayable)
            return false;
        const QImage image = frameToImage(displayable);
        if (image.isNull())
            return false;
        displayFrame(image);
        updatePositionUi();
    }

    return true;
}

enum AVPixelFormat VideoPlayer::getHwFormatCallback(AVCodecContext *ctx, const enum AVPixelFormat *pixFmts)
{
    auto *self = static_cast<VideoPlayer*>(ctx->opaque);
    if (!self || self->m_hwPixFmt == AV_PIX_FMT_NONE)
        return pixFmts[0];
    for (const enum AVPixelFormat *p = pixFmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == self->m_hwPixFmt)
            return *p;
    }
    qWarning() << "HW pixel format not offered by decoder, falling back to SW";
    return pixFmts[0];
}

AVFrame *VideoPlayer::ensureSwFrame(AVFrame *frame)
{
    if (!frame)
        return nullptr;
    if (m_hwPixFmt == AV_PIX_FMT_NONE || frame->format != m_hwPixFmt)
        return frame;
    if (!m_swFrame)
        return nullptr;

    av_frame_unref(m_swFrame);
    if (av_hwframe_transfer_data(m_swFrame, frame, 0) < 0) {
        qWarning() << "av_hwframe_transfer_data failed";
        return nullptr;
    }
    m_swFrame->pts = frame->pts;
    m_swFrame->best_effort_timestamp = frame->best_effort_timestamp;
    return m_swFrame;
}

QImage VideoPlayer::frameToImage(const AVFrame *frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        qWarning() << "frameToImage: invalid frame";
        return {};
    }

    m_swsCtx = sws_getCachedContext(
        m_swsCtx,
        frame->width,
        frame->height,
        static_cast<AVPixelFormat>(frame->format),
        frame->width,
        frame->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);

    if (!m_swsCtx) {
        qWarning() << "frameToImage: sws_getCachedContext failed";
        return {};
    }

    QImage image(frame->width, frame->height, QImage::Format_RGB888);
    if (image.isNull()) {
        qWarning() << "frameToImage: QImage alloc failed" << frame->width << "x" << frame->height;
        return {};
    }

    uint8_t *dest[4] = { image.bits(), nullptr, nullptr, nullptr };
    int destLinesize[4] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };
    sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, dest, destLinesize);
    return image;
}

int64_t VideoPlayer::streamFrameDurationUs() const
{
    if (!m_formatCtx || m_videoStreamIndex < 0)
        return AV_TIME_BASE / 30;

    AVStream *stream = m_formatCtx->streams[m_videoStreamIndex];
    AVRational frameRate = stream->avg_frame_rate;
    if (frameRate.num <= 0 || frameRate.den <= 0)
        frameRate = stream->r_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0)
        return qMax<int64_t>(1, av_rescale_q(1, av_inv_q(frameRate), AV_TIME_BASE_Q));

    return AV_TIME_BASE / 30;
}

int64_t VideoPlayer::streamTimestampForPosition(int64_t positionUs) const
{
    if (!m_formatCtx || m_videoStreamIndex < 0)
        return positionUs;

    AVStream *stream = m_formatCtx->streams[m_videoStreamIndex];
    int64_t timestamp = av_rescale_q(positionUs, AV_TIME_BASE_Q, stream->time_base);
    if (stream->start_time != AV_NOPTS_VALUE)
        timestamp += stream->start_time;
    return timestamp;
}

int64_t VideoPlayer::positionFromStreamTimestamp(int64_t timestamp) const
{
    if (!m_formatCtx || m_videoStreamIndex < 0)
        return timestamp;

    AVStream *stream = m_formatCtx->streams[m_videoStreamIndex];
    if (stream->start_time != AV_NOPTS_VALUE)
        timestamp -= stream->start_time;
    return av_rescale_q(timestamp, stream->time_base, AV_TIME_BASE_Q);
}

int VideoPlayer::sliderPositionForUs(int64_t positionUs) const
{
    const int64_t positionMs = qMax<int64_t>(0, positionUs / 1000);
    return static_cast<int>(qMin<int64_t>(positionMs, std::numeric_limits<int>::max()));
}

double VideoPlayer::streamDisplayAspectRatio() const
{
    if (!m_formatCtx || !m_codecCtx || m_videoStreamIndex < 0 || m_codecCtx->height <= 0)
        return 0.0;

    AVStream *stream = m_formatCtx->streams[m_videoStreamIndex];
    AVRational sampleAspect = av_guess_sample_aspect_ratio(m_formatCtx, stream, nullptr);
    if (sampleAspect.num <= 0 || sampleAspect.den <= 0)
        sampleAspect = stream->sample_aspect_ratio;
    if (sampleAspect.num <= 0 || sampleAspect.den <= 0)
        sampleAspect = m_codecCtx->sample_aspect_ratio;

    double aspectRatio = static_cast<double>(m_codecCtx->width) / m_codecCtx->height;
    if (sampleAspect.num > 0 && sampleAspect.den > 0)
        aspectRatio *= av_q2d(sampleAspect);

    return (aspectRatio > 0.0 && std::isfinite(aspectRatio)) ? aspectRatio : 0.0;
}

double VideoPlayer::effectiveDisplayAspectRatio() const
{
    if (m_displayAspectRatio > 0.0 && std::isfinite(m_displayAspectRatio))
        return m_displayAspectRatio;

    if (!m_currentFrameImage.isNull() && m_currentFrameImage.height() > 0)
        return static_cast<double>(m_currentFrameImage.width()) / m_currentFrameImage.height();

    return 0.0;
}

QSize VideoPlayer::fittedDisplaySize(const QSize &bounds) const
{
    if (!bounds.isValid())
        return QSize(1, 1);

    const double aspectRatio = effectiveDisplayAspectRatio();
    if (!(aspectRatio > 0.0) || !std::isfinite(aspectRatio))
        return bounds;

    int targetWidth = bounds.width();
    int targetHeight = qRound(targetWidth / aspectRatio);
    if (targetHeight > bounds.height()) {
        targetHeight = bounds.height();
        targetWidth = qRound(targetHeight * aspectRatio);
    }

    return QSize(qMax(1, targetWidth), qMax(1, targetHeight));
}

void VideoPlayer::refreshDisplayedFrame()
{
    if (m_currentFrameImage.isNull())
        return;

    displayFrame(m_currentFrameImage);
}

void VideoPlayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    refreshDisplayedFrame();
}

void VideoPlayer::handlePlaybackTick()
{
    if (!m_playing)
        return;

    bool advanced = false;
    if (m_playbackSpeed >= 0.0) {
        advanced = decodeNextFrame(true);
    } else {
        const int64_t stepUs = qMax<int64_t>(1, m_frameDurationUs);
        const int64_t targetUs = qMax<int64_t>(0, m_currentPositionUs - stepUs);
        advanced = seekInternal(targetUs, true, true);
    }

    // A/V sync — audio (QMediaPlayer) is the master clock. Strategy:
    //   * audioAhead > 2000 ms : do a single direct seek (cheaper than drops).
    //   * audioAhead > 600 ms  : drop ONE frame per tick (gentle catch-up).
    //   * audioAhead in [-300, 600] ms : do nothing (acceptable drift).
    //   * audioAhead < -300 ms : push audio forward (never rewind).
    // Dropping more than one frame per tick or running every tick produces a
    // visibly choppy video (the earlier behaviour). Single-frame drops at a
    // higher threshold trade marginal lag for smooth motion.
    if (m_playbackSpeed >= 0.0 && m_audioPlayer && m_audioPlayer->source().isValid()) {
        const qint64 videoMs = m_currentPositionUs / 1000;
        const qint64 audioMs = m_audioPlayer->position();
        const qint64 audioAhead = audioMs - videoMs;

        if (audioAhead > 2000) {
            qInfo() << "VideoPlayer video seek-forward audioAhead=" << audioAhead
                    << "video=" << videoMs << "audio=" << audioMs;
            seekInternal(audioMs * 1000, true, false);
        } else if (audioAhead > 600) {
            // Drop exactly ONE frame this tick — gentle catch-up.
            decodeNextFrame(false);
        } else if (audioAhead < -300) {
            qInfo() << "VideoPlayer audio nudge-forward audioAhead=" << audioAhead
                    << "video=" << videoMs << "audio=" << audioMs;
            m_audioPlayer->setPosition(videoMs);
        }
    }

    // Sequence mode: detect when we've crossed the active entry's outPoint or
    // run off the end of the file, and switch to the next entry.
    if (sequenceActive() && m_activeEntry >= 0 && m_activeEntry < m_sequence.size()
        && m_playbackSpeed >= 0.0) {
        const auto &active = m_sequence[m_activeEntry];
        const int64_t entryEndLocalUs = static_cast<int64_t>(active.clipOut * AV_TIME_BASE);
        const bool reachedEntryEnd = (m_currentPositionUs >= entryEndLocalUs);
        const bool decodeStopped = !advanced;

        if (reachedEntryEnd || decodeStopped) {
            const int nextIdx = m_activeEntry + 1;
            if (nextIdx < m_sequence.size()) {
                if (advanceToEntry(nextIdx)) {
                    updatePositionUi();
                    scheduleNextFrame();
                    return;
                }
            } else {
                // End of sequence.
                m_timelinePositionUs = m_sequenceDurationUs;
                m_currentPositionUs = entryEndLocalUs;
                updatePositionUi();
                pause();
                if (m_audioPlayer) m_audioPlayer->stop();
                return;
            }
        }
    }

    if (!advanced) {
        if (m_playbackSpeed >= 0.0 && m_durationUs > 0) {
            m_currentPositionUs = m_durationUs;
            updatePositionUi();
        }
        pause();
        if (m_audioPlayer)
            m_audioPlayer->stop();
        return;
    }

    scheduleNextFrame();
}
