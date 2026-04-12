#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QFileInfo>
#include <QVector>
#include <QMenu>
#include "VideoEffect.h"
#include "Keyframe.h"
#include "WaveformGenerator.h"
#include "TextManager.h"
#include "PlaybackTypes.h"

class UndoManager;
class PlayheadOverlay;
struct TimelineState;

struct ClipInfo {
    QString filePath;
    QString displayName;
    double duration;
    double inPoint = 0.0;
    double outPoint = 0.0;
    double leadInSec = 0.0; // leading gap before the clip on the timeline, grows on left-trim to keep the right edge fixed
    double speed = 1.0;   // 0.25x - 4.0x
    double volume = 1.0;  // 0.0 - 2.0 (0=mute, 1=normal, 2=boost)

    // Phase 3: Color correction, effects, keyframes
    ColorCorrection colorCorrection;
    QVector<VideoEffect> effects;
    KeyframeManager keyframes;

    // Phase 5: Waveform
    WaveformData waveform;

    // Phase 6: Enhanced text overlays
    TextManager textManager;

    double effectiveDuration() const {
        double out = (outPoint > 0.0) ? outPoint : duration;
        return (out - inPoint) / speed;
    }
};

enum class DragMode {
    None,
    TrimLeft,
    TrimRight,
    MoveClip
};

class TimelineTrack : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineTrack(QWidget *parent = nullptr);

    void addClip(const ClipInfo &clip);
    void insertClip(int index, const ClipInfo &clip);
    void removeClip(int index);
    void moveClip(int fromIndex, int toIndex);
    void splitClipAt(int index, double localSeconds);
    int clipCount() const { return m_clips.size(); }
    const QVector<ClipInfo> &clips() const { return m_clips; }
    void setClips(const QVector<ClipInfo> &clips);

    void setSelectedClip(int index);
    void toggleClipSelection(int index);
    void clearClipSelection();
    void moveSelectedClipsGroup(int targetIndex);
    int selectedClip() const { return m_selectedClips.isEmpty() ? -1 : m_selectedClips.last(); }
    const QList<int> &selectedClips() const { return m_selectedClips; }
    bool isClipSelected(int index) const { return m_selectedClips.contains(index); }

    int clipAtX(int x) const;
    double xToSeconds(int x) const;
    int secondsToX(double seconds) const;
    int clipStartX(int index) const;

    void setSnapEnabled(bool enabled) { m_snapEnabled = enabled; }
    bool snapEnabled() const { return m_snapEnabled; }
    void setPixelsPerSecond(double pps);
    double pixelsPerSecond() const { return m_pixelsPerSecond; }
    void setRowHeight(int h);
    int rowHeight() const { return m_rowHeight; }
    void setMuted(bool muted) { m_muted = muted; update(); }
    bool isMuted() const { return m_muted; }
    void setSolo(bool solo) { m_solo = solo; update(); }
    bool isSolo() const { return m_solo; }
    void setHidden(bool hidden) { m_hidden = hidden; update(); }
    bool isHidden() const { return m_hidden; }

signals:
    void clipClicked(int index);
    void selectionChanged(int primaryIndex, bool additive);
    void emptyAreaClicked();
    void clipMoved(int fromIndex, int toIndex);
    void modified();
    void seekRequested(double seconds);
    void rowHeightChanged(int newHeight);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateMinimumWidth();
    int snapToEdge(int x) const;

    QVector<ClipInfo> m_clips;
    QList<int> m_selectedClips;
    DragMode m_dragMode = DragMode::None;
    int m_dragClipIndex = -1;
    int m_dragStartX = 0;
    double m_dragOriginalValue = 0.0;
    double m_dragOriginalLeadIn = 0.0;
    bool m_snapEnabled = true;
    int m_dropTargetIndex = -1;

    double m_pixelsPerSecond = 10.0;
    bool m_muted = false;
    bool m_solo = false;
    bool m_hidden = false;
    int m_rowHeight = 50; // adjustable per-track via setRowHeight
    bool m_resizingHeight = false;
    int m_resizeStartY = 0;
    int m_resizeStartHeight = 0;
    static constexpr int TRIM_HANDLE_WIDTH = 6;
    static constexpr int SNAP_THRESHOLD = 8;
    static constexpr int RESIZE_HANDLE_HEIGHT = 6;
};

class Timeline : public QWidget
{
    Q_OBJECT

public:
    explicit Timeline(QWidget *parent = nullptr);

    void addClip(const QString &filePath);
    void splitAtPlayhead();
    void deleteSelectedClip();
    void rippleDeleteSelectedClip();
    bool hasSelection() const;

    // Copy / Paste
    void copySelectedClip();
    void pasteClip();
    bool hasClipboard() const { return m_clipboard.has_value(); }

    // Undo / Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Snap
    void setSnapEnabled(bool enabled);
    bool snapEnabled() const;

    // Zoom
    void zoomIn();
    void zoomOut();
    void setZoomLevel(double pixelsPerSecond);

    // I/O markers
    void markIn();
    void markOut();
    double markedIn() const { return m_markIn; }
    double markedOut() const { return m_markOut; }
    bool hasMarkedRange() const { return m_markIn >= 0 && m_markOut > m_markIn; }

    // Multi-track
    void addVideoTrack();
    void addAudioTrack();
    int videoTrackCount() const { return m_videoTracks.size(); }
    int audioTrackCount() const { return m_audioTracks.size(); }

    // Track row height (applied to all tracks AND their header widgets)
    void setTrackHeight(int h);
    int trackHeight() const { return m_trackHeight; }
    void increaseTrackHeight();
    void decreaseTrackHeight();

    // Clip speed & volume
    void setClipSpeed(double speed);
    void setClipVolume(double volume);

    // Phase 3: Color correction, effects, keyframes
    void setClipColorCorrection(const ColorCorrection &cc);
    void setClipEffects(const QVector<VideoEffect> &effects);
    void setClipKeyframes(const KeyframeManager &km);
    ColorCorrection clipColorCorrection() const;
    QVector<VideoEffect> clipEffects() const;
    KeyframeManager clipKeyframes() const;
    double selectedClipDuration() const;

    // Audio
    void addAudioFile(const QString &filePath);
    void toggleMuteTrack(int audioTrackIndex);
    void toggleSoloTrack(int audioTrackIndex);

    void setPlayheadPosition(double seconds);
    double playheadPosition() const { return m_playheadPos; }
    double totalDuration() const;
    const QVector<ClipInfo> &videoClips() const {
        static const QVector<ClipInfo> kEmpty;
        if (m_videoTracks.isEmpty() || !m_videoTracks.first())
            return kEmpty;
        return m_videoTracks.first()->clips();
    }

    UndoManager *undoManager() const { return m_undoManager; }

    // Multi-clip playback: flatten all video tracks into a sorted, gap-aware
    // schedule with topmost-track-wins resolution (Premiere V1/V2 semantics).
    QVector<PlaybackEntry> computePlaybackSequence() const;

    // Project save/load support
    QVector<QVector<ClipInfo>> allVideoTracks() const;
    QVector<QVector<ClipInfo>> allAudioTracks() const;
    void restoreFromProject(const QVector<QVector<ClipInfo>> &videoTracks,
                            const QVector<QVector<ClipInfo>> &audioTracks,
                            double playhead, double markIn, double markOut, int zoom);

signals:
    void clipSelected(int index);
    void scrubPositionChanged(double seconds);
    void positionChanged(double seconds);
    void sequenceChanged(const QVector<PlaybackEntry> &entries);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTrackClipClicked(int index);
    void onTrackModified();

private:
    void setupUI();
    void saveUndoState(const QString &description);
    void restoreState(const TimelineState &state);
    TimelineState currentState() const;
    void syncPlayheadOverlay();
    void updateInfoLabel();
    void ensureSequenceFitsViewport();
    QWidget *createTrackHeader(TimelineTrack *track, const QString &name, bool isAudioRow);
    void notifyMutationsChanged();
    void wireTrackSelection(TimelineTrack *track);
    void clearAllSelections();

    QVector<TimelineTrack*> m_videoTracks;
    QVector<TimelineTrack*> m_audioTracks;
    TimelineTrack *m_videoTrack = nullptr; // alias for m_videoTracks[0]
    TimelineTrack *m_audioTrack = nullptr; // alias for m_audioTracks[0]
    QScrollArea *m_scrollArea;
    QWidget *m_tracksWidget;
    QVBoxLayout *m_tracksLayout;
    QWidget *m_headerColumn = nullptr;
    QVBoxLayout *m_headerLayout = nullptr;
    static constexpr int kHeaderColumnWidth = 130;
    PlayheadOverlay *m_playheadOverlay = nullptr;
    class TimeRuler *m_timeRuler = nullptr;
    QLabel *m_infoLabel;
    double m_playheadPos = 0.0;
    double m_markIn = -1.0;
    double m_markOut = -1.0;
    double m_zoomLevel = 10.0; // pixels per second (double so we can go sub-1 for long clips)
    int m_trackHeight = 50; // default row height for new and existing tracks

    UndoManager *m_undoManager;
    std::optional<ClipInfo> m_clipboard;
};

class PlayheadOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit PlayheadOverlay(QWidget *parent = nullptr);
    void setPlayheadX(int x);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void playheadMoved(int x);
    void playheadReleased(int x);

private:
    int m_playheadX = 0;
    bool m_dragging = false;
};

class TimeRuler : public QWidget
{
    Q_OBJECT

public:
    explicit TimeRuler(QWidget *parent = nullptr);
    void setPixelsPerSecond(double pps);
    double pixelsPerSecond() const { return m_pixelsPerSecond; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void zoomChanged(double newPixelsPerSecond);

private:
    double m_pixelsPerSecond = 10.0;
    bool m_dragging = false;
    int m_dragStartX = 0;
    double m_dragStartPps = 10.0;
};
