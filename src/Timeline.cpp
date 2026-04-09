#include "Timeline.h"
#include "UndoManager.h"
#include <optional>

extern "C" {
#include <libavformat/avformat.h>
}

// --- PlayheadOverlay ---

PlayheadOverlay::PlayheadOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void PlayheadOverlay::setPlayheadX(int x)
{
    m_playheadX = x;
    update();
}

void PlayheadOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0xFF, 0x44, 0x44), 2));
    painter.drawLine(m_playheadX, 0, m_playheadX, height());
    QPolygon triangle;
    triangle << QPoint(m_playheadX - 6, 0)
             << QPoint(m_playheadX + 6, 0)
             << QPoint(m_playheadX, 10);
    painter.setBrush(QColor(0xFF, 0x44, 0x44));
    painter.drawPolygon(triangle);
}

void PlayheadOverlay::mousePressEvent(QMouseEvent *event)
{
    m_dragging = true;
    m_playheadX = event->pos().x();
    emit playheadMoved(m_playheadX);
    update();
}

void PlayheadOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_playheadX = qMax(0, event->pos().x());
        emit playheadMoved(m_playheadX);
        update();
    }
}

void PlayheadOverlay::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

// --- TimelineTrack ---

TimelineTrack::TimelineTrack(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(CLIP_HEIGHT);
    setMaximumHeight(CLIP_HEIGHT);
    setMouseTracking(true);
    setAcceptDrops(true);
}

void TimelineTrack::addClip(const ClipInfo &clip)
{
    m_clips.append(clip);
    updateMinimumWidth();
    update();
    emit modified();
}

void TimelineTrack::insertClip(int index, const ClipInfo &clip)
{
    if (index < 0 || index > m_clips.size()) index = m_clips.size();
    m_clips.insert(index, clip);
    updateMinimumWidth();
    update();
    emit modified();
}

void TimelineTrack::removeClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    m_clips.removeAt(index);
    if (m_selectedClip >= m_clips.size())
        m_selectedClip = m_clips.size() - 1;
    updateMinimumWidth();
    update();
    emit selectionChanged(m_selectedClip);
    emit modified();
}

void TimelineTrack::moveClip(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_clips.size()) return;
    if (toIndex < 0 || toIndex >= m_clips.size()) return;
    if (fromIndex == toIndex) return;

    ClipInfo clip = m_clips[fromIndex];
    m_clips.removeAt(fromIndex);
    m_clips.insert(toIndex, clip);
    m_selectedClip = toIndex;
    updateMinimumWidth();
    update();
    emit clipMoved(fromIndex, toIndex);
    emit modified();
}

void TimelineTrack::splitClipAt(int index, double localSeconds)
{
    if (index < 0 || index >= m_clips.size()) return;
    ClipInfo &original = m_clips[index];
    double effectiveStart = original.inPoint;
    double effectiveEnd = (original.outPoint > 0.0) ? original.outPoint : original.duration;
    double splitPoint = effectiveStart + localSeconds;
    if (splitPoint <= effectiveStart + 0.1 || splitPoint >= effectiveEnd - 0.1) return;

    ClipInfo secondHalf = original;
    secondHalf.inPoint = splitPoint;
    secondHalf.outPoint = effectiveEnd;
    original.outPoint = splitPoint;

    m_clips.insert(index + 1, secondHalf);
    updateMinimumWidth();
    update();
    emit modified();
}

void TimelineTrack::setClips(const QVector<ClipInfo> &clips)
{
    m_clips = clips;
    updateMinimumWidth();
    update();
}

void TimelineTrack::setSelectedClip(int index)
{
    m_selectedClip = index;
    update();
    emit selectionChanged(index);
}

int TimelineTrack::clipAtX(int x) const
{
    int cx = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        int clipWidth = qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
        if (x >= cx && x < cx + clipWidth) return i;
        cx += clipWidth;
    }
    return -1;
}

int TimelineTrack::clipStartX(int index) const
{
    int x = 0;
    for (int i = 0; i < index && i < m_clips.size(); ++i)
        x += qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
    return x;
}

double TimelineTrack::xToSeconds(int x) const
{
    return static_cast<double>(x) / PIXELS_PER_SECOND;
}

int TimelineTrack::secondsToX(double seconds) const
{
    return static_cast<int>(seconds * PIXELS_PER_SECOND);
}

int TimelineTrack::snapToEdge(int x) const
{
    if (!m_snapEnabled) return x;
    int cx = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        if (qAbs(x - cx) <= SNAP_THRESHOLD) return cx;
        cx += qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
        if (qAbs(x - cx) <= SNAP_THRESHOLD) return cx;
    }
    return x;
}

void TimelineTrack::updateMinimumWidth()
{
    int totalWidth = 0;
    for (const auto &c : m_clips)
        totalWidth += qMax(20, static_cast<int>(c.effectiveDuration() * PIXELS_PER_SECOND));
    setMinimumWidth(totalWidth + 100);
}

void TimelineTrack::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int x = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        int clipWidth = qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
        QRect clipRect(x, 0, clipWidth, CLIP_HEIGHT);

        QColor color = (i % 2 == 0) ? QColor(0x44, 0x88, 0xCC) : QColor(0x44, 0xAA, 0x88);
        if (i == m_selectedClip) color = color.lighter(140);

        // Drop target indicator
        if (m_dragMode == DragMode::MoveClip && i == m_dropTargetIndex) {
            painter.setPen(QPen(Qt::yellow, 3));
            painter.drawLine(x, 0, x, CLIP_HEIGHT);
        }

        painter.fillRect(clipRect, color);

        if (i == m_selectedClip) {
            painter.setPen(QPen(Qt::yellow, 2));
            painter.drawRect(clipRect.adjusted(1, 1, -1, -1));
            painter.fillRect(QRect(x, 0, TRIM_HANDLE_WIDTH, CLIP_HEIGHT), QColor(255, 255, 255, 80));
            painter.fillRect(QRect(x + clipWidth - TRIM_HANDLE_WIDTH, 0, TRIM_HANDLE_WIDTH, CLIP_HEIGHT), QColor(255, 255, 255, 80));
        } else {
            painter.setPen(QPen(Qt::white, 1));
            painter.drawRect(clipRect);
        }

        // Snap indicator lines
        if (m_snapEnabled) {
            painter.setPen(QPen(QColor(0xFF, 0xAA, 0x00, 60), 1, Qt::DashLine));
            painter.drawLine(x, 0, x, CLIP_HEIGHT);
        }

        painter.setPen(Qt::white);
        QRect textRect = clipRect.adjusted(8, 4, -8, -4);
        double dur = m_clips[i].effectiveDuration();
        int mins = static_cast<int>(dur) / 60;
        int secs = static_cast<int>(dur) % 60;
        QString label = m_clips[i].displayName + QString(" [%1:%2]").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
            painter.fontMetrics().elidedText(label, Qt::ElideRight, textRect.width()));

        x += clipWidth;
    }
}

void TimelineTrack::mousePressEvent(QMouseEvent *event)
{
    int clickedClip = clipAtX(event->pos().x());

    if (event->button() == Qt::LeftButton && clickedClip >= 0) {
        setSelectedClip(clickedClip);
        emit clipClicked(clickedClip);

        int cx = clipStartX(clickedClip);
        int clipWidth = qMax(20, static_cast<int>(m_clips[clickedClip].effectiveDuration() * PIXELS_PER_SECOND));
        int localX = event->pos().x() - cx;

        if (localX <= TRIM_HANDLE_WIDTH) {
            m_dragMode = DragMode::TrimLeft;
            m_dragClipIndex = clickedClip;
            m_dragStartX = event->pos().x();
            m_dragOriginalValue = m_clips[clickedClip].inPoint;
        } else if (localX >= clipWidth - TRIM_HANDLE_WIDTH) {
            m_dragMode = DragMode::TrimRight;
            m_dragClipIndex = clickedClip;
            m_dragStartX = event->pos().x();
            double out = m_clips[clickedClip].outPoint > 0 ? m_clips[clickedClip].outPoint : m_clips[clickedClip].duration;
            m_dragOriginalValue = out;
        } else {
            // Start drag-to-reorder
            m_dragMode = DragMode::MoveClip;
            m_dragClipIndex = clickedClip;
            m_dragStartX = event->pos().x();
            m_dropTargetIndex = -1;
        }
    } else if (clickedClip < 0) {
        setSelectedClip(-1);
    }
}

void TimelineTrack::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode == DragMode::MoveClip && m_dragClipIndex >= 0) {
        // Drag to reorder
        int targetClip = clipAtX(event->pos().x());
        if (targetClip < 0) {
            // Past end of clips
            targetClip = m_clips.size() - 1;
        }
        m_dropTargetIndex = targetClip;
        setCursor(Qt::ClosedHandCursor);
        update();
        return;
    }

    if (m_dragMode == DragMode::TrimLeft || m_dragMode == DragMode::TrimRight) {
        int snappedX = snapToEdge(event->pos().x());
        int dx = snappedX - m_dragStartX;
        double deltaSec = static_cast<double>(dx) / PIXELS_PER_SECOND;
        ClipInfo &clip = m_clips[m_dragClipIndex];

        if (m_dragMode == DragMode::TrimLeft) {
            double newIn = qMax(0.0, m_dragOriginalValue + deltaSec);
            double maxIn = (clip.outPoint > 0 ? clip.outPoint : clip.duration) - 0.1;
            clip.inPoint = qMin(newIn, maxIn);
        } else {
            double newOut = qMin(clip.duration, m_dragOriginalValue + deltaSec);
            double minOut = clip.inPoint + 0.1;
            clip.outPoint = qMax(newOut, minOut);
        }
        updateMinimumWidth();
        update();
        return;
    }

    // Hover cursor
    int hoverClip = clipAtX(event->pos().x());
    if (hoverClip >= 0 && hoverClip == m_selectedClip) {
        int cx = clipStartX(hoverClip);
        int clipWidth = qMax(20, static_cast<int>(m_clips[hoverClip].effectiveDuration() * PIXELS_PER_SECOND));
        int localX = event->pos().x() - cx;
        if (localX <= TRIM_HANDLE_WIDTH || localX >= clipWidth - TRIM_HANDLE_WIDTH)
            setCursor(Qt::SizeHorCursor);
        else
            setCursor(Qt::OpenHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void TimelineTrack::mouseReleaseEvent(QMouseEvent *)
{
    if (m_dragMode == DragMode::MoveClip && m_dragClipIndex >= 0 && m_dropTargetIndex >= 0) {
        if (m_dragClipIndex != m_dropTargetIndex)
            moveClip(m_dragClipIndex, m_dropTargetIndex);
    }
    if (m_dragMode == DragMode::TrimLeft || m_dragMode == DragMode::TrimRight) {
        emit modified();
    }
    m_dragMode = DragMode::None;
    m_dragClipIndex = -1;
    m_dropTargetIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

// --- Timeline ---

Timeline::Timeline(QWidget *parent)
    : QWidget(parent)
{
    m_undoManager = new UndoManager(this);
    setupUI();
    saveUndoState("Initial state");
}

void Timeline::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_infoLabel = new QLabel("Timeline", this);
    m_infoLabel->setStyleSheet("font-weight: bold; color: #ccc;");
    layout->addWidget(m_infoLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("background-color: #2a2a2a;");

    auto *tracksContainer = new QWidget();
    auto *tracksOuterLayout = new QVBoxLayout(tracksContainer);
    tracksOuterLayout->setContentsMargins(0, 0, 0, 0);
    tracksOuterLayout->setSpacing(0);

    auto *playheadOverlay = new PlayheadOverlay(tracksContainer);
    playheadOverlay->setFixedHeight(15);
    playheadOverlay->setStyleSheet("background-color: #222;");
    connect(playheadOverlay, &PlayheadOverlay::playheadMoved, this, [this](int x) {
        m_playheadPos = m_videoTrack->xToSeconds(x);
        emit positionChanged(m_playheadPos);
    });
    tracksOuterLayout->addWidget(playheadOverlay);

    auto *tracksWidget = new QWidget();
    auto *tracksLayout = new QVBoxLayout(tracksWidget);
    tracksLayout->setSpacing(2);

    auto *videoLabel = new QLabel("V1");
    videoLabel->setStyleSheet("color: #4488CC; font-size: 11px;");
    m_videoTrack = new TimelineTrack(this);

    auto *audioLabel = new QLabel("A1");
    audioLabel->setStyleSheet("color: #44AA88; font-size: 11px;");
    m_audioTrack = new TimelineTrack(this);

    tracksLayout->addWidget(videoLabel);
    tracksLayout->addWidget(m_videoTrack);
    tracksLayout->addWidget(audioLabel);
    tracksLayout->addWidget(m_audioTrack);
    tracksLayout->addStretch();

    tracksOuterLayout->addWidget(tracksWidget);
    m_scrollArea->setWidget(tracksContainer);
    layout->addWidget(m_scrollArea);
    setStyleSheet("background-color: #333;");

    connect(m_videoTrack, &TimelineTrack::clipClicked, this, &Timeline::onTrackClipClicked);
    connect(m_videoTrack, &TimelineTrack::selectionChanged, this, [this](int index) {
        m_audioTrack->setSelectedClip(index);
        emit clipSelected(index);
    });
    connect(m_videoTrack, &TimelineTrack::modified, this, &Timeline::onTrackModified);
    connect(m_audioTrack, &TimelineTrack::modified, this, &Timeline::onTrackModified);
}

void Timeline::addClip(const QString &filePath)
{
    AVFormatContext *fmt = nullptr;
    double duration = 0.0;
    if (avformat_open_input(&fmt, filePath.toUtf8().constData(), nullptr, nullptr) == 0) {
        if (avformat_find_stream_info(fmt, nullptr) >= 0)
            duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
        avformat_close_input(&fmt);
    }

    ClipInfo clip;
    clip.filePath = filePath;
    clip.displayName = QFileInfo(filePath).fileName();
    clip.duration = duration;
    clip.inPoint = 0.0;
    clip.outPoint = 0.0;

    m_videoTrack->addClip(clip);
    m_audioTrack->addClip(clip);
    saveUndoState("Add clip");
    updateInfoLabel();
}

void Timeline::splitAtPlayhead()
{
    const auto &clips = m_videoTrack->clips();
    double accum = 0.0;
    for (int i = 0; i < clips.size(); ++i) {
        double clipDur = clips[i].effectiveDuration();
        if (m_playheadPos >= accum && m_playheadPos < accum + clipDur) {
            double localPos = m_playheadPos - accum;
            m_videoTrack->splitClipAt(i, localPos);
            m_audioTrack->splitClipAt(i, localPos);
            saveUndoState("Split clip");
            updateInfoLabel();
            return;
        }
        accum += clipDur;
    }
}

void Timeline::deleteSelectedClip()
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0) return;
    m_videoTrack->removeClip(sel);
    m_audioTrack->removeClip(sel);
    saveUndoState("Delete clip");
    updateInfoLabel();
}

void Timeline::rippleDeleteSelectedClip()
{
    // Same as delete — clips are sequential so removing automatically ripples
    deleteSelectedClip();
}

bool Timeline::hasSelection() const
{
    return m_videoTrack->selectedClip() >= 0;
}

void Timeline::copySelectedClip()
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0 || sel >= m_videoTrack->clips().size()) return;
    m_clipboard = m_videoTrack->clips()[sel];
}

void Timeline::pasteClip()
{
    if (!m_clipboard.has_value()) return;

    int insertAt = m_videoTrack->selectedClip() + 1;
    if (insertAt <= 0) insertAt = m_videoTrack->clipCount();

    m_videoTrack->insertClip(insertAt, m_clipboard.value());
    m_audioTrack->insertClip(insertAt, m_clipboard.value());
    m_videoTrack->setSelectedClip(insertAt);
    m_audioTrack->setSelectedClip(insertAt);
    saveUndoState("Paste clip");
    updateInfoLabel();
}

void Timeline::undo()
{
    if (!canUndo()) return;
    TimelineState state = m_undoManager->undo();
    restoreState(state);
    updateInfoLabel();
}

void Timeline::redo()
{
    if (!canRedo()) return;
    TimelineState state = m_undoManager->redo();
    restoreState(state);
    updateInfoLabel();
}

bool Timeline::canUndo() const { return m_undoManager->canUndo(); }
bool Timeline::canRedo() const { return m_undoManager->canRedo(); }

void Timeline::setSnapEnabled(bool enabled)
{
    m_videoTrack->setSnapEnabled(enabled);
    m_audioTrack->setSnapEnabled(enabled);
}

bool Timeline::snapEnabled() const { return m_videoTrack->snapEnabled(); }

void Timeline::setPlayheadPosition(double seconds)
{
    m_playheadPos = seconds;
}

double Timeline::totalDuration() const
{
    double total = 0.0;
    for (const auto &clip : m_videoTrack->clips())
        total += clip.effectiveDuration();
    return total;
}

void Timeline::onTrackClipClicked(int index)
{
    m_audioTrack->setSelectedClip(index);
    emit clipSelected(index);
}

void Timeline::onTrackModified()
{
    // Auto-save after drag operations are handled by explicit saveUndoState calls
}

void Timeline::saveUndoState(const QString &description)
{
    m_undoManager->saveState(currentState(), description);
}

TimelineState Timeline::currentState() const
{
    TimelineState state;
    state.videoClips = m_videoTrack->clips();
    state.audioClips = m_audioTrack->clips();
    state.selectedClip = m_videoTrack->selectedClip();
    state.playheadPos = m_playheadPos;
    return state;
}

void Timeline::restoreState(const TimelineState &state)
{
    m_videoTrack->setClips(state.videoClips);
    m_audioTrack->setClips(state.audioClips);
    m_videoTrack->setSelectedClip(state.selectedClip);
    m_audioTrack->setSelectedClip(state.selectedClip);
    m_playheadPos = state.playheadPos;
    emit clipSelected(state.selectedClip);
}

void Timeline::updateInfoLabel()
{
    m_infoLabel->setText(QString("Timeline - %1 clip(s) | %2")
        .arg(m_videoTrack->clipCount())
        .arg(snapEnabled() ? "Snap ON" : "Snap OFF"));
}
