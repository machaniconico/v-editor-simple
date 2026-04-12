#include "Timeline.h"
#include "UndoManager.h"
#include <optional>
#include <cmath>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QWheelEvent>
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
}

// --- PlayheadOverlay ---

PlayheadOverlay::PlayheadOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void PlayheadOverlay::setPlayheadX(int x) { m_playheadX = x; update(); }

void PlayheadOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0xFF, 0x44, 0x44), 2));
    painter.drawLine(m_playheadX, 0, m_playheadX, height());
    QPolygon tri;
    tri << QPoint(m_playheadX - 6, 0) << QPoint(m_playheadX + 6, 0) << QPoint(m_playheadX, 10);
    painter.setBrush(QColor(0xFF, 0x44, 0x44));
    painter.drawPolygon(tri);
}

void PlayheadOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_playheadX = qBound(0, event->pos().x(), width());
    grabMouse();
    emit playheadMoved(m_playheadX);
    event->accept();
    update();
}

void PlayheadOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    m_playheadX = qBound(0, event->pos().x(), width());
    emit playheadMoved(m_playheadX);
    event->accept();
    update();
}

void PlayheadOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    releaseMouse();
    m_playheadX = qBound(0, event->pos().x(), width());
    emit playheadReleased(m_playheadX);
    event->accept();
    update();
}

// --- TimeRuler ---

TimeRuler::TimeRuler(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(22);
    setStyleSheet("background-color: #2d2d2d;");
    setMouseTracking(true);
    setCursor(Qt::SizeHorCursor);
    setToolTip("Drag horizontally to zoom timeline (right=in, left=out)");
}

void TimeRuler::setPixelsPerSecond(double pps)
{
    pps = qBound(0.1, pps, 200.0);
    if (qFuzzyCompare(pps, m_pixelsPerSecond))
        return;
    m_pixelsPerSecond = pps;
    update();
}

void TimeRuler::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0x2d, 0x2d, 0x2d));
    painter.setPen(QPen(QColor(0xaa, 0xaa, 0xaa), 1));

    const int w = width();
    const int h = height();

    int majorIntervalSec = 1;
    if (m_pixelsPerSecond < 0.2) majorIntervalSec = 3600;       // 1h marks for 4h+ clips
    else if (m_pixelsPerSecond < 0.5) majorIntervalSec = 1800;  // 30 min
    else if (m_pixelsPerSecond < 1.0) majorIntervalSec = 600;   // 10 min
    else if (m_pixelsPerSecond < 2.0) majorIntervalSec = 300;
    else if (m_pixelsPerSecond < 3.0) majorIntervalSec = 120;
    else if (m_pixelsPerSecond < 4.0) majorIntervalSec = 60;
    else if (m_pixelsPerSecond < 8.0) majorIntervalSec = 30;
    else if (m_pixelsPerSecond < 16.0) majorIntervalSec = 10;
    else if (m_pixelsPerSecond < 40.0) majorIntervalSec = 5;
    else if (m_pixelsPerSecond < 80.0) majorIntervalSec = 2;

    const int majorPx = qMax(1, static_cast<int>(majorIntervalSec * m_pixelsPerSecond));
    if (majorPx <= 0) return;

    const int minorPerMajor = 5;
    const int minorPx = qMax(1, majorPx / minorPerMajor);

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (int x = 0; x < w; x += minorPx) {
        const bool isMajor = (x % majorPx) == 0;
        if (isMajor) {
            painter.setPen(QPen(QColor(0xdd, 0xdd, 0xdd), 1));
            painter.drawLine(x, h - 8, x, h);
            const int totalSec = static_cast<int>(x / m_pixelsPerSecond);
            const int hours = totalSec / 3600;
            const int mins = (totalSec % 3600) / 60;
            const int secs = totalSec % 60;
            QString label = (hours > 0)
                ? QString("%1:%2:%3").arg(hours).arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'))
                : QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
            painter.drawText(QRect(x + 2, 0, 60, h - 8), Qt::AlignLeft | Qt::AlignVCenter, label);
        } else {
            painter.setPen(QPen(QColor(0x77, 0x77, 0x77), 1));
            painter.drawLine(x, h - 4, x, h);
        }
    }

    painter.setPen(QPen(QColor(0x55, 0x55, 0x55), 1));
    painter.drawLine(0, h - 1, w, h - 1);
}

void TimeRuler::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragging = true;
    m_dragStartX = event->pos().x();
    m_dragStartPps = m_pixelsPerSecond;
    grabMouse();
    event->accept();
}

void TimeRuler::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const int dx = event->pos().x() - m_dragStartX;
    // Multiplicative drag: each pixel multiplies zoom by ~2%. Works uniformly
    // from 0.1 pps (4h clip fits) up to 200 pps (frame-precise).
    const double factor = std::pow(1.02, static_cast<double>(dx));
    const double newPps = qBound(0.1, m_dragStartPps * factor, 200.0);
    if (!qFuzzyCompare(newPps, m_pixelsPerSecond)) {
        m_pixelsPerSecond = newPps;
        emit zoomChanged(newPps);
        update();
    }
    event->accept();
}

void TimeRuler::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
    releaseMouse();
    event->accept();
}

// --- TimelineTrack ---

TimelineTrack::TimelineTrack(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(m_rowHeight);
    setMaximumHeight(m_rowHeight);
    setMouseTracking(true);
    setAcceptDrops(true);
}

void TimelineTrack::setRowHeight(int h)
{
    h = qBound(20, h, 300);
    if (h == m_rowHeight) return;
    m_rowHeight = h;
    setMinimumHeight(m_rowHeight);
    setMaximumHeight(m_rowHeight);
    update();
}

void TimelineTrack::addClip(const ClipInfo &clip) { m_clips.append(clip); updateMinimumWidth(); update(); emit modified(); }
void TimelineTrack::insertClip(int index, const ClipInfo &clip)
{
    if (index < 0 || index > m_clips.size()) index = m_clips.size();
    m_clips.insert(index, clip); updateMinimumWidth(); update(); emit modified();
}

void TimelineTrack::removeClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    m_clips.removeAt(index);
    QList<int> newSel;
    for (int s : m_selectedClips) {
        if (s < index) newSel.append(s);
        else if (s > index) newSel.append(s - 1);
    }
    m_selectedClips = newSel;
    const int primary = m_selectedClips.isEmpty() ? -1 : m_selectedClips.last();
    updateMinimumWidth(); update(); emit selectionChanged(primary, false); emit modified();
}

void TimelineTrack::moveClip(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_clips.size()) return;
    if (toIndex < 0 || toIndex >= m_clips.size()) return;
    if (fromIndex == toIndex) return;
    ClipInfo clip = m_clips[fromIndex];
    m_clips.removeAt(fromIndex);
    m_clips.insert(toIndex, clip);
    m_selectedClips = {toIndex};
    updateMinimumWidth(); update();
    emit selectionChanged(toIndex, false);
    emit clipMoved(fromIndex, toIndex); emit modified();
}

void TimelineTrack::splitClipAt(int index, double localSeconds)
{
    if (index < 0 || index >= m_clips.size()) return;
    ClipInfo &original = m_clips[index];
    double effectiveStart = original.inPoint;
    double effectiveEnd = (original.outPoint > 0.0) ? original.outPoint : original.duration;
    double splitPoint = effectiveStart + localSeconds * original.speed;
    if (splitPoint <= effectiveStart + 0.1 || splitPoint >= effectiveEnd - 0.1) return;
    ClipInfo secondHalf = original;
    secondHalf.inPoint = splitPoint;
    secondHalf.outPoint = effectiveEnd;
    original.outPoint = splitPoint;
    m_clips.insert(index + 1, secondHalf);
    updateMinimumWidth(); update(); emit modified();
}

void TimelineTrack::setClips(const QVector<ClipInfo> &clips) { m_clips = clips; updateMinimumWidth(); update(); }
void TimelineTrack::setSelectedClip(int index) {
    QList<int> newSel;
    if (index >= 0) newSel.append(index);
    if (m_selectedClips == newSel) return;
    m_selectedClips = newSel;
    update();
    emit selectionChanged(index, false);
}

void TimelineTrack::toggleClipSelection(int index) {
    if (index < 0) return;
    if (m_selectedClips.contains(index)) {
        m_selectedClips.removeAll(index);
    } else {
        m_selectedClips.append(index);
    }
    update();
    const int primary = m_selectedClips.isEmpty() ? -1 : m_selectedClips.last();
    emit selectionChanged(primary, true);
}

void TimelineTrack::clearClipSelection() {
    if (m_selectedClips.isEmpty()) return;
    m_selectedClips.clear();
    update();
    emit selectionChanged(-1, false);
}

void TimelineTrack::moveSelectedClipsGroup(int targetIndex)
{
    if (m_selectedClips.size() < 2) return;
    if (targetIndex < 0 || targetIndex > m_clips.size()) return;

    QList<int> sortedSel = m_selectedClips;
    std::sort(sortedSel.begin(), sortedSel.end());

    // Bail if any source index is invalid.
    for (int idx : sortedSel)
        if (idx < 0 || idx >= m_clips.size()) return;

    // Extract moved clips in order.
    QVector<ClipInfo> movedClips;
    movedClips.reserve(sortedSel.size());
    for (int idx : sortedSel) movedClips.append(m_clips[idx]);

    // Compute insert position relative to the list with sources removed.
    int adjustedTarget = targetIndex;
    for (int idx : sortedSel)
        if (idx < targetIndex) adjustedTarget--;

    // Remove sources from highest to lowest so indices stay valid.
    for (int i = sortedSel.size() - 1; i >= 0; --i)
        m_clips.removeAt(sortedSel[i]);

    adjustedTarget = qBound(0, adjustedTarget, m_clips.size());

    for (int i = 0; i < movedClips.size(); ++i)
        m_clips.insert(adjustedTarget + i, movedClips[i]);

    // New selection is the consecutive range of inserted clips.
    QList<int> newSel;
    for (int i = 0; i < movedClips.size(); ++i) newSel.append(adjustedTarget + i);
    m_selectedClips = newSel;

    updateMinimumWidth(); update();
    emit selectionChanged(newSel.last(), false);
    emit clipMoved(sortedSel.first(), adjustedTarget);
    emit modified();
}

void TimelineTrack::setPixelsPerSecond(double pps)
{
    m_pixelsPerSecond = qBound(0.1, pps, 200.0);
    updateMinimumWidth();
    update();
}

int TimelineTrack::clipAtX(int x) const
{
    int cx = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        cx += qMax(0, static_cast<int>(m_clips[i].leadInSec * m_pixelsPerSecond));
        int w = qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * m_pixelsPerSecond));
        if (x >= cx && x < cx + w) return i;
        cx += w;
    }
    return -1;
}

int TimelineTrack::clipStartX(int index) const
{
    int x = 0;
    for (int i = 0; i < index && i < m_clips.size(); ++i) {
        x += qMax(0, static_cast<int>(m_clips[i].leadInSec * m_pixelsPerSecond));
        x += qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * m_pixelsPerSecond));
    }
    if (index >= 0 && index < m_clips.size())
        x += qMax(0, static_cast<int>(m_clips[index].leadInSec * m_pixelsPerSecond));
    return x;
}

double TimelineTrack::xToSeconds(int x) const { return static_cast<double>(x) / m_pixelsPerSecond; }
int TimelineTrack::secondsToX(double seconds) const { return static_cast<int>(seconds * m_pixelsPerSecond); }

int TimelineTrack::snapToEdge(int x) const
{
    if (!m_snapEnabled) return x;
    int cx = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        cx += qMax(0, static_cast<int>(m_clips[i].leadInSec * m_pixelsPerSecond));
        if (qAbs(x - cx) <= SNAP_THRESHOLD) return cx;
        cx += qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * m_pixelsPerSecond));
        if (qAbs(x - cx) <= SNAP_THRESHOLD) return cx;
    }
    return x;
}

void TimelineTrack::updateMinimumWidth()
{
    qint64 totalWidth = 0;
    for (const auto &c : m_clips) {
        totalWidth += qMax<qint64>(0, static_cast<qint64>(c.leadInSec * m_pixelsPerSecond));
        qint64 w = qMax<qint64>(20,
            static_cast<qint64>(c.effectiveDuration() * m_pixelsPerSecond));
        totalWidth += w;
    }
    // Qt widget width is int; cap to a sane upper bound. Large widgets break
    // backing-store allocation and painter clipping, but we need to allow
    // multi-clip sequences to span well past the original ~5h cap. Timeline's
    // ensureSequenceFitsViewport() auto-zooms-out when content exceeds the
    // viewport so the user normally never hits this hard cap.
    constexpr qint64 kMaxWidth = 2000000;
    totalWidth = qMin(totalWidth, kMaxWidth);
    setMinimumWidth(static_cast<int>(totalWidth) + 100);
}

void TimelineTrack::paintEvent(QPaintEvent *event)
{
    static int paintCount = 0;
    if (++paintCount <= 5) {
        qInfo() << "TimelineTrack::paintEvent #" << paintCount
                << "clips=" << m_clips.size() << "pps=" << m_pixelsPerSecond;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect visibleRect = event ? event->rect() : QRect(0, 0, width(), height());
    int x = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        // Leading gap (leadInSec) before this clip, created by left-trim so the
        // clip's right edge stays anchored. Skip drawing over it — the dark
        // track background shows through.
        x += qMax(0, static_cast<int>(m_clips[i].leadInSec * m_pixelsPerSecond));
        // Cap the drawn width so pathological long clips don't explode paint
        // (raised in lockstep with updateMinimumWidth's kMaxWidth so multi-clip
        // sequences place subsequent clips at the correct x position).
        int clipWidth = qMax(20, static_cast<int>(
            qMin<double>(2000000.0,
                         m_clips[i].effectiveDuration() * m_pixelsPerSecond)));
        QRect clipRect(x, 0, clipWidth, m_rowHeight);
        QColor color = (i % 2 == 0) ? QColor(0x44, 0x88, 0xCC) : QColor(0x44, 0xAA, 0x88);
        const bool isSelected = m_selectedClips.contains(i);
        if (isSelected) color = color.lighter(140);
        if (m_dragMode == DragMode::MoveClip && i == m_dropTargetIndex) {
            painter.setPen(QPen(Qt::yellow, 3));
            painter.drawLine(x, 0, x, m_rowHeight);
        }
        painter.fillRect(clipRect, color);
        // Trim handle indicators — visible on all clips so users can discover
        // the edge-drag-to-trim affordance without having to hunt for it.
        const QColor trimColor = isSelected ? QColor(255, 200, 60, 220)
                                            : QColor(255, 255, 255, 120);
        painter.fillRect(QRect(x, 0, TRIM_HANDLE_WIDTH, m_rowHeight), trimColor);
        painter.fillRect(QRect(x + clipWidth - TRIM_HANDLE_WIDTH, 0, TRIM_HANDLE_WIDTH, m_rowHeight), trimColor);
        if (isSelected) {
            painter.setPen(QPen(Qt::yellow, 2));
            painter.drawRect(clipRect.adjusted(1, 1, -1, -1));
        } else {
            painter.setPen(QPen(Qt::white, 1));
            painter.drawRect(clipRect);
        }
        // Draw waveform if available — only for the visible x range to avoid
        // drawing 100k+ lines for very long clips.
        if (!m_clips[i].waveform.isEmpty()) {
            painter.setPen(QPen(QColor(100, 200, 150, 180), 1));
            const auto &wf = m_clips[i].waveform;
            const int wfPeakCount = wf.peaks.size();
            if (wfPeakCount > 0 && clipWidth > 2) {
                const int midY = m_rowHeight / 2;
                const int maxAmp = m_rowHeight / 2 - 4;

                // Intersect with visible rect to skip drawing offscreen pixels.
                const int pxStart = qMax(0, visibleRect.left() - x);
                const int pxEnd   = qMin(clipWidth, visibleRect.right() - x + 1);

                for (int px = pxStart; px < pxEnd; ++px) {
                    // Map px (0..clipWidth-1) linearly to peaks (0..wfPeakCount-1).
                    qint64 peakIdx = static_cast<qint64>(px) * wfPeakCount / clipWidth;
                    if (peakIdx < 0 || peakIdx >= wfPeakCount) continue;
                    const float amp = wf.peaks[static_cast<int>(peakIdx)];
                    const int h = static_cast<int>(amp * maxAmp);
                    painter.drawLine(x + px, midY - h, x + px, midY + h);
                }
            }
        }

        // Label with speed indicator
        painter.setPen(Qt::white);
        QRect textRect = clipRect.adjusted(8, 4, -8, -4);
        double dur = m_clips[i].effectiveDuration();
        int mins = static_cast<int>(dur) / 60;
        int secs = static_cast<int>(dur) % 60;
        QString label = m_clips[i].displayName;
        if (m_clips[i].speed != 1.0)
            label += QString(" [%1x]").arg(m_clips[i].speed, 0, 'f', 1);
        label += QString(" %1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
            painter.fontMetrics().elidedText(label, Qt::ElideRight, textRect.width()));
        x += clipWidth;
    }

    // Visual indication of hidden / muted state — overlay the whole track.
    if (m_hidden) {
        painter.fillRect(rect(), QColor(0, 0, 0, 140));
        painter.setPen(QPen(QColor(180, 180, 180), 1, Qt::DashLine));
        painter.drawLine(0, 0, width(), height());
    }
    if (m_muted) {
        painter.fillRect(rect(), QColor(120, 30, 30, 60));
    }

    // Resize handle hint at the bottom edge of the track. A faint horizontal
    // bar tells the user the row height can be dragged here.
    painter.fillRect(0, height() - 2, width(), 2, QColor(70, 70, 70));
}

void TimelineTrack::mousePressEvent(QMouseEvent *event)
{
    // Bottom-edge drag → row height resize. Detected before the seek/click
    // handling so the user can resize even when clicking inside a clip area.
    if (event->button() == Qt::LeftButton
        && event->pos().y() >= m_rowHeight - RESIZE_HANDLE_HEIGHT) {
        m_resizingHeight = true;
        m_resizeStartY = event->globalPosition().toPoint().y();
        m_resizeStartHeight = m_rowHeight;
        setCursor(Qt::SizeVerCursor);
        grabMouse();
        event->accept();
        return;
    }

    const bool additive = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);

    if (event->button() == Qt::LeftButton && !additive)
        emit seekRequested(qMax(0.0, xToSeconds(event->pos().x())));

    int clickedClip = clipAtX(event->pos().x());
    if (event->button() == Qt::LeftButton && clickedClip >= 0) {
        if (additive) {
            toggleClipSelection(clickedClip);
            emit clipClicked(clickedClip);
            return;
        }
        // Preserve a multi-selection when the user starts a group drag by
        // clicking (without modifier) on a clip that's already part of it.
        // Otherwise, replace the selection with the clicked clip.
        const bool partOfMulti = m_selectedClips.size() > 1
                                 && m_selectedClips.contains(clickedClip);
        if (!partOfMulti) setSelectedClip(clickedClip);
        emit clipClicked(clickedClip);
        int cx = clipStartX(clickedClip);
        int clipWidth = qMax(20, static_cast<int>(m_clips[clickedClip].effectiveDuration() * m_pixelsPerSecond));
        int localX = event->pos().x() - cx;
        if (localX <= TRIM_HANDLE_WIDTH) {
            m_dragMode = DragMode::TrimLeft; m_dragClipIndex = clickedClip;
            m_dragStartX = event->pos().x(); m_dragOriginalValue = m_clips[clickedClip].inPoint;
            m_dragOriginalLeadIn = m_clips[clickedClip].leadInSec;
        } else if (localX >= clipWidth - TRIM_HANDLE_WIDTH) {
            m_dragMode = DragMode::TrimRight; m_dragClipIndex = clickedClip;
            m_dragStartX = event->pos().x();
            m_dragOriginalValue = m_clips[clickedClip].outPoint > 0 ? m_clips[clickedClip].outPoint : m_clips[clickedClip].duration;
        } else {
            m_dragMode = DragMode::MoveClip; m_dragClipIndex = clickedClip;
            m_dragStartX = event->pos().x(); m_dropTargetIndex = -1;
        }
    } else if (clickedClip < 0 && !additive) {
        setSelectedClip(-1);
        emit emptyAreaClicked();
    }
}

void TimelineTrack::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizingHeight) {
        const int globalY = event->globalPosition().toPoint().y();
        const int delta = globalY - m_resizeStartY;
        const int newH = qBound(20, m_resizeStartHeight + delta, 300);
        if (newH != m_rowHeight) {
            setRowHeight(newH);
            emit rowHeightChanged(newH);
        }
        event->accept();
        return;
    }
    if (m_dragMode == DragMode::MoveClip && m_dragClipIndex >= 0 && m_dragClipIndex < m_clips.size()) {
        // Fixed 40 px step per reorder — every 40 px of drag advances the
        // drop target by one slot. Predictable and independent of clip widths
        // so long clips can still be reordered.
        constexpr int STEP_PX = 40;
        const int dx = event->pos().x() - m_dragStartX;
        int target = m_dragClipIndex;
        if (dx > 0) {
            const int steps = dx / STEP_PX;
            target = qMin(m_dragClipIndex + steps, m_clips.size() - 1);
        } else if (dx < 0) {
            const int steps = (-dx) / STEP_PX;
            target = qMax(m_dragClipIndex - steps, 0);
        }
        m_dropTargetIndex = target;
        setCursor(Qt::ClosedHandCursor); update(); return;
    }
    if (m_dragMode == DragMode::TrimLeft || m_dragMode == DragMode::TrimRight) {
        if (m_dragClipIndex < 0 || m_dragClipIndex >= m_clips.size()) {
            m_dragMode = DragMode::None;
            m_dragClipIndex = -1;
            setCursor(Qt::ArrowCursor);
            return;
        }
        int snappedX = snapToEdge(event->pos().x());
        int dx = snappedX - m_dragStartX;
        double deltaSec = static_cast<double>(dx) / m_pixelsPerSecond;
        ClipInfo &clip = m_clips[m_dragClipIndex];
        if (m_dragMode == DragMode::TrimLeft) {
            double newIn = qMax(0.0, m_dragOriginalValue + deltaSec);
            double maxIn = (clip.outPoint > 0 ? clip.outPoint : clip.duration) - 0.1;
            clip.inPoint = qMin(newIn, maxIn);
            // Keep the clip's right edge anchored by shifting its leadInSec by
            // the same amount the inPoint changed. inPoint+ → leadInSec+
            // (clip slides right), inPoint− → leadInSec− (clip slides left,
            // clamped at 0 so it can't cross its neighbor).
            const double inPointDelta = clip.inPoint - m_dragOriginalValue;
            clip.leadInSec = qMax(0.0, m_dragOriginalLeadIn + inPointDelta);
        } else {
            double newOut = qMin(clip.duration, m_dragOriginalValue + deltaSec);
            clip.outPoint = qMax(newOut, clip.inPoint + 0.1);
        }
        updateMinimumWidth(); update(); return;
    }
    // Hover near the bottom edge → vertical resize cursor hint.
    if (event->pos().y() >= m_rowHeight - RESIZE_HANDLE_HEIGHT) {
        setCursor(Qt::SizeVerCursor);
        return;
    }

    int hover = clipAtX(event->pos().x());
    if (hover >= 0 && m_selectedClips.contains(hover)) {
        int cx = clipStartX(hover);
        int cw = qMax(20, static_cast<int>(m_clips[hover].effectiveDuration() * m_pixelsPerSecond));
        int lx = event->pos().x() - cx;
        setCursor((lx <= TRIM_HANDLE_WIDTH || lx >= cw - TRIM_HANDLE_WIDTH) ? Qt::SizeHorCursor : Qt::OpenHandCursor);
    } else { setCursor(Qt::ArrowCursor); }
}

void TimelineTrack::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizingHeight) {
        m_resizingHeight = false;
        releaseMouse();
        setCursor(Qt::ArrowCursor);
        emit rowHeightChanged(m_rowHeight);
        if (event) event->accept();
        return;
    }
    if (m_dragMode == DragMode::MoveClip
        && m_dragClipIndex >= 0 && m_dragClipIndex < m_clips.size()
        && m_dropTargetIndex >= 0 && m_dropTargetIndex < m_clips.size()) {
        if (m_selectedClips.size() > 1) {
            moveSelectedClipsGroup(m_dropTargetIndex);
        } else if (m_dragClipIndex != m_dropTargetIndex) {
            moveClip(m_dragClipIndex, m_dropTargetIndex);
        }
    }
    if (m_dragMode == DragMode::TrimLeft || m_dragMode == DragMode::TrimRight) emit modified();
    m_dragMode = DragMode::None; m_dragClipIndex = -1; m_dropTargetIndex = -1;
    setCursor(Qt::ArrowCursor); update();
}

// --- Timeline ---

Timeline::Timeline(QWidget *parent) : QWidget(parent)
{
    m_undoManager = new UndoManager(this);
    setupUI();
    saveUndoState("Initial state");
}

void Timeline::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *infoRow = new QHBoxLayout();
    m_infoLabel = new QLabel("Timeline", this);
    m_infoLabel->setStyleSheet("font-weight: bold; color: #ccc;");
    infoRow->addWidget(m_infoLabel, 1);

    // Track row-height controls. The minus/plus buttons let the user resize
    // every track in 10 px steps (clamped to 30..200) for dense or expanded
    // multi-track layouts.
    const QString sizeBtnStyle =
        "QPushButton { background-color: #444; color: #ddd; border: 1px solid #666;"
        "  border-radius: 3px; font-size: 13px; padding: 0 6px; }"
        "QPushButton:hover { background-color: #555; }";
    auto *rowHLabel = new QLabel(QStringLiteral("行高"), this);
    rowHLabel->setStyleSheet("color: #999; font-size: 11px;");
    auto *rowMinus = new QPushButton(QString::fromUtf8("\xE2\x88\x92"), this); // −
    auto *rowPlus  = new QPushButton(QString::fromUtf8("\x2B"), this);          // +
    rowMinus->setFixedSize(26, 22);
    rowPlus->setFixedSize(26, 22);
    rowMinus->setStyleSheet(sizeBtnStyle);
    rowPlus->setStyleSheet(sizeBtnStyle);
    rowMinus->setToolTip(QStringLiteral("行を低く"));
    rowPlus->setToolTip(QStringLiteral("行を高く"));
    connect(rowMinus, &QPushButton::clicked, this, &Timeline::decreaseTrackHeight);
    connect(rowPlus,  &QPushButton::clicked, this, &Timeline::increaseTrackHeight);
    infoRow->addWidget(rowHLabel);
    infoRow->addWidget(rowMinus);
    infoRow->addWidget(rowPlus);

    layout->addLayout(infoRow);

    // Horizontal split: [frozen header column | scrollable tracks area].
    // Track headers (mute/hide buttons + label) live in the header column on
    // the left and stay put while the user scrolls the tracks horizontally.
    auto *contentArea = new QWidget(this);
    auto *contentHbox = new QHBoxLayout(contentArea);
    contentHbox->setContentsMargins(0, 0, 0, 0);
    contentHbox->setSpacing(0);

    m_headerColumn = new QWidget(contentArea);
    m_headerColumn->setFixedWidth(kHeaderColumnWidth);
    m_headerColumn->setStyleSheet("background-color: #252525;");
    m_headerLayout = new QVBoxLayout(m_headerColumn);
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(2);
    // Magnet (snap) toggle in the header column's top area. Fixed height
    // matches the time ruler (22) + playhead overlay (15) so V1's header
    // aligns with the first track widget below.
    auto *magnetArea = new QWidget(m_headerColumn);
    magnetArea->setFixedHeight(22 + 15);
    magnetArea->setStyleSheet("background-color: #1f1f1f;");
    auto *magnetRow = new QHBoxLayout(magnetArea);
    magnetRow->setContentsMargins(6, 2, 6, 2);
    magnetRow->setSpacing(0);
    auto *magnetBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\xA7\xB2"), magnetArea); // 🧲
    magnetBtn->setCheckable(true);
    magnetBtn->setChecked(snapEnabled());
    magnetBtn->setFixedSize(32, 26);
    magnetBtn->setToolTip(QStringLiteral("マグネット / スナップ 切替"));
    magnetBtn->setStyleSheet(
        "QPushButton { background-color: #444; color: #ddd; border: 1px solid #666;"
        "  border-radius: 3px; font-size: 14px; padding: 0; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:checked { background-color: #3a7f4a; color: white;"
        "  border: 1px solid #6cbd7a; }");
    connect(magnetBtn, &QPushButton::toggled, this, [this](bool on) {
        setSnapEnabled(on);
    });
    magnetRow->addWidget(magnetBtn);
    magnetRow->addStretch();
    m_headerLayout->addWidget(magnetArea);

    m_scrollArea = new QScrollArea(contentArea);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("background-color: #2a2a2a;");
    // Horizontal scrollbar removed — its role overlaps with the player's seek
    // bar above. Auto-fit zoom makes content fit the viewport in most cases;
    // the scroll position is still controllable via wheel / drag.
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *tracksContainer = new QWidget();
    auto *tracksOuterLayout = new QVBoxLayout(tracksContainer);
    tracksOuterLayout->setContentsMargins(0, 0, 0, 0);
    tracksOuterLayout->setSpacing(0);

    m_timeRuler = new TimeRuler(tracksContainer);
    m_timeRuler->setPixelsPerSecond(m_zoomLevel);
    connect(m_timeRuler, &TimeRuler::zoomChanged, this, &Timeline::setZoomLevel);
    tracksOuterLayout->addWidget(m_timeRuler);

    m_playheadOverlay = new PlayheadOverlay(tracksContainer);
    m_playheadOverlay->setFixedHeight(15);
    m_playheadOverlay->setStyleSheet("background-color: #222;");
    connect(m_playheadOverlay, &PlayheadOverlay::playheadMoved, this, [this](int x) {
        m_playheadPos = m_videoTrack->xToSeconds(x);
        emit scrubPositionChanged(m_playheadPos);
    });
    connect(m_playheadOverlay, &PlayheadOverlay::playheadReleased, this, [this](int x) {
        m_playheadPos = m_videoTrack->xToSeconds(x);
        emit positionChanged(m_playheadPos);
    });
    tracksOuterLayout->addWidget(m_playheadOverlay);

    m_tracksWidget = new QWidget();
    m_tracksLayout = new QVBoxLayout(m_tracksWidget);
    m_tracksLayout->setContentsMargins(0, 0, 0, 0);
    m_tracksLayout->setSpacing(2);

    // Create initial V1 and A1.
    m_videoTrack = new TimelineTrack(this);
    m_audioTrack = new TimelineTrack(this);
    m_videoTracks.append(m_videoTrack);
    m_audioTracks.append(m_audioTrack);

    m_headerLayout->addWidget(createTrackHeader(m_videoTrack, "V1", false));
    m_tracksLayout->addWidget(m_videoTrack);

    m_headerLayout->addWidget(createTrackHeader(m_audioTrack, "A1", true));
    m_tracksLayout->addWidget(m_audioTrack);

    m_tracksLayout->addStretch();
    m_headerLayout->addStretch();

    tracksOuterLayout->addWidget(m_tracksWidget);
    m_scrollArea->setWidget(tracksContainer);

    // Install event filters so clicks on empty areas (below tracks, outer
    // container, scroll viewport) deselect all clips like clicks on a track's
    // empty area do.
    m_tracksWidget->installEventFilter(this);
    tracksContainer->installEventFilter(this);
    m_scrollArea->viewport()->installEventFilter(this);

    contentHbox->addWidget(m_headerColumn);
    contentHbox->addWidget(m_scrollArea, 1);

    layout->addWidget(contentArea);
    setStyleSheet("background-color: #333;");

    wireTrackSelection(m_videoTrack);
    wireTrackSelection(m_audioTrack);

    auto seekToTrackClick = [this](double seconds) {
        m_playheadPos = qMax(0.0, seconds);
        syncPlayheadOverlay();
        emit positionChanged(m_playheadPos);
    };
    connect(m_videoTrack, &TimelineTrack::seekRequested, this, seekToTrackClick);
    connect(m_audioTrack, &TimelineTrack::seekRequested, this, seekToTrackClick);

    syncPlayheadOverlay();
}

QWidget *Timeline::createTrackHeader(TimelineTrack *track, const QString &name, bool isAudioRow)
{
    auto *w = new QWidget();
    w->setFixedHeight(m_trackHeight); // mirrors current row height
    w->setStyleSheet("QWidget { background-color: #303030; border-right: 1px solid #444; }");

    auto *hbox = new QHBoxLayout(w);
    hbox->setContentsMargins(4, 4, 4, 4);
    hbox->setSpacing(3);

    // Speaker icon (audio mute toggle). Uses Unicode glyphs that render in
    // Segoe UI on Windows: 🔊 normal / 🔇 muted (we keep the same glyph and
    // rely on the checked state's red background to indicate muted).
    auto *muteBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x8A"), w); // 🔊
    muteBtn->setFixedSize(28, 28);
    muteBtn->setCheckable(true);
    muteBtn->setToolTip(QStringLiteral("ミュート (audio)"));
    muteBtn->setStyleSheet(
        "QPushButton { background-color: #444; color: #ddd; border: 1px solid #666;"
        "  border-radius: 3px; font-size: 14px; padding: 0; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:checked { background-color: #c44; color: white; border: 1px solid #f88; }");

    // Visibility toggle. ◉ (circled bullet) reads as an "eye with iris" /
    // silhouette and is more compact than the full 👁 emoji. ◯ (large circle)
    // is the empty/closed-eye state.
    auto *hideBtn = new QPushButton(QString::fromUtf8("\xE2\x97\x89"), w); // ◉
    hideBtn->setFixedSize(28, 28);
    hideBtn->setCheckable(true);
    hideBtn->setToolTip(QStringLiteral("非表示 (hide video)"));
    hideBtn->setStyleSheet(
        "QPushButton { background-color: #444; color: #ddd; border: 1px solid #666;"
        "  border-radius: 3px; font-size: 16px; padding: 0; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:checked { background-color: #666; color: #888; border: 1px solid #999; }");

    auto *label = new QLabel(name, w);
    label->setStyleSheet(isAudioRow
        ? "QLabel { background: transparent; border: none; color: #44AA88; font-weight: bold; font-size: 12px; }"
        : "QLabel { background: transparent; border: none; color: #4488CC; font-weight: bold; font-size: 12px; }");

    hbox->addWidget(muteBtn);
    hbox->addWidget(hideBtn);
    hbox->addWidget(label, 1);

    QPointer<TimelineTrack> trackPtr(track);
    QPointer<QPushButton> muteBtnPtr(muteBtn);
    QPointer<QPushButton> hideBtnPtr(hideBtn);
    QPointer<QWidget> headerPtr(w);

    // When the user drags the bottom edge of THIS track, resize the matching
    // header on the left so the row stays aligned.
    connect(track, &TimelineTrack::rowHeightChanged, this, [headerPtr](int newH) {
        if (headerPtr) headerPtr->setFixedHeight(newH);
    });

    connect(muteBtn, &QPushButton::toggled, this, [this, trackPtr, muteBtnPtr](bool checked) {
        if (!trackPtr) return;
        qInfo() << "Timeline: mute toggled =" << checked << "track=" << trackPtr.data();
        trackPtr->setMuted(checked);
        if (muteBtnPtr) {
            muteBtnPtr->setText(checked
                ? QString::fromUtf8("\xF0\x9F\x94\x87")    // 🔇
                : QString::fromUtf8("\xF0\x9F\x94\x8A")); // 🔊
        }
        // Re-emit sequence so VideoPlayer picks up the audioMuted flag on the
        // currently active entry. setSequence is fast in the no-file-switch case.
        emit sequenceChanged(computePlaybackSequence());
    });
    connect(hideBtn, &QPushButton::toggled, this, [this, trackPtr, hideBtnPtr](bool checked) {
        if (!trackPtr) return;
        qInfo() << "Timeline: hide toggled =" << checked << "track=" << trackPtr.data();
        trackPtr->setHidden(checked);
        if (hideBtnPtr) {
            // ◉ open eye (visible) / ⊘ struck-out (hidden)
            hideBtnPtr->setText(checked
                ? QString::fromUtf8("\xE2\x8A\x98")    // ⊘
                : QString::fromUtf8("\xE2\x97\x89")); // ◉
        }
        emit sequenceChanged(computePlaybackSequence());
    });

    return w;
}

void Timeline::addVideoTrack()
{
    int num = m_videoTracks.size() + 1;
    auto *track = new TimelineTrack(this);
    track->setPixelsPerSecond(m_zoomLevel);
    track->setSnapEnabled(snapEnabled());
    track->setRowHeight(m_trackHeight);

    // tracksLayout starts with widgets directly (no leading spacer), but
    // m_headerLayout starts with an addSpacing(...) item that aligns the
    // first header with the first track widget. Compensate with +1 on the
    // header insert index so V2 ends up below V1, not above.
    const int trackIdx = m_videoTracks.size();
    m_tracksLayout->insertWidget(trackIdx, track);
    m_headerLayout->insertWidget(trackIdx + 1,
        createTrackHeader(track, QString("V%1").arg(num), false));

    m_videoTracks.append(track);
    wireTrackSelection(track);
    updateInfoLabel();
}

void Timeline::addAudioTrack()
{
    int num = m_audioTracks.size() + 1;
    auto *track = new TimelineTrack(this);
    track->setPixelsPerSecond(m_zoomLevel);
    track->setSnapEnabled(snapEnabled());
    track->setRowHeight(m_trackHeight);

    // Insert at the end of the audio block (just before the trailing stretch
    // of each layout). Both layouts have a trailing stretch so count()-1 is
    // the right insert position for either.
    const int trackInsertIdx = m_tracksLayout->count() - 1;
    const int headerInsertIdx = m_headerLayout->count() - 1;
    m_tracksLayout->insertWidget(trackInsertIdx, track);
    m_headerLayout->insertWidget(headerInsertIdx,
        createTrackHeader(track, QString("A%1").arg(num), true));

    m_audioTracks.append(track);
    wireTrackSelection(track);
    updateInfoLabel();
}

void Timeline::addClip(const QString &filePath)
{
    AVFormatContext *fmt = nullptr;
    double duration = 0.0;
    if (avformat_open_input(&fmt, filePath.toUtf8().constData(), nullptr, nullptr) == 0) {
        if (avformat_find_stream_info(fmt, nullptr) >= 0 && fmt->duration > 0)
            duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
        avformat_close_input(&fmt);
    }
    ClipInfo clip;
    clip.filePath = filePath;
    clip.displayName = QFileInfo(filePath).fileName();
    clip.duration = duration;

    // Premiere-style stacking: each drop goes onto the first EMPTY video track
    // (creating a new V<n>/A<n> track if every existing track is occupied).
    // The first drop fills V1/A1, the second drop fills V2/A2, and so on.
    int videoTrackIdx = -1;
    for (int t = 0; t < m_videoTracks.size(); ++t) {
        if (m_videoTracks[t] && m_videoTracks[t]->clipCount() == 0) {
            videoTrackIdx = t;
            break;
        }
    }
    if (videoTrackIdx < 0) {
        addVideoTrack();
        videoTrackIdx = m_videoTracks.size() - 1;
    }

    int audioTrackIdx = -1;
    for (int t = 0; t < m_audioTracks.size(); ++t) {
        if (m_audioTracks[t] && m_audioTracks[t]->clipCount() == 0) {
            audioTrackIdx = t;
            break;
        }
    }
    if (audioTrackIdx < 0) {
        addAudioTrack();
        audioTrackIdx = m_audioTracks.size() - 1;
    }

    qInfo() << "Timeline::addClip routing video→V" << (videoTrackIdx + 1)
            << "audio→A" << (audioTrackIdx + 1)
            << "file=" << filePath;

    // Generate waveform async; apply to the AUDIO track that received the clip.
    auto *wfGen = new WaveformGenerator(this);
    QPointer<Timeline> self(this);
    const int waveAudioIdx = audioTrackIdx;
    connect(wfGen, &WaveformGenerator::waveformReady, this,
        [self, wfGen, waveAudioIdx](const QString &path, const WaveformData &data) {
            qInfo() << "Timeline::waveformReady slot peaks=" << data.peaks.size()
                    << "dur=" << data.duration << "audioIdx=" << waveAudioIdx;
            if (self && waveAudioIdx >= 0 && waveAudioIdx < self->m_audioTracks.size()) {
                auto *track = self->m_audioTracks[waveAudioIdx];
                if (track) {
                    auto clips = track->clips();
                    for (int i = 0; i < clips.size(); ++i) {
                        if (clips[i].filePath == path && clips[i].waveform.isEmpty()) {
                            clips[i].waveform = data;
                            track->setClips(clips);
                            break;
                        }
                    }
                }
            }
            wfGen->deleteLater();
        });
    wfGen->generateAsync(filePath);

    if (videoTrackIdx >= 0 && videoTrackIdx < m_videoTracks.size())
        m_videoTracks[videoTrackIdx]->addClip(clip);
    if (audioTrackIdx >= 0 && audioTrackIdx < m_audioTracks.size())
        m_audioTracks[audioTrackIdx]->addClip(clip);
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

void Timeline::rippleDeleteSelectedClip() { deleteSelectedClip(); }
bool Timeline::hasSelection() const { return m_videoTrack->selectedClip() >= 0; }

void Timeline::copySelectedClip()
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0 || sel >= m_videoTrack->clips().size()) return;
    m_clipboard = m_videoTrack->clips()[sel];
}

void Timeline::pasteClip()
{
    if (!m_clipboard.has_value()) return;
    if (!m_videoTrack || !m_audioTrack) return;
    int insertAt = m_videoTrack->selectedClip() + 1;
    if (insertAt <= 0) insertAt = m_videoTrack->clipCount();
    const int maxIndex = qMin(m_videoTrack->clipCount(), m_audioTrack->clipCount());
    insertAt = qBound(0, insertAt, maxIndex);
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
    restoreState(m_undoManager->undo());
    updateInfoLabel();
}

void Timeline::redo()
{
    if (!canRedo()) return;
    restoreState(m_undoManager->redo());
    updateInfoLabel();
}

bool Timeline::canUndo() const { return m_undoManager->canUndo(); }
bool Timeline::canRedo() const { return m_undoManager->canRedo(); }

void Timeline::setSnapEnabled(bool enabled)
{
    for (auto *t : m_videoTracks) t->setSnapEnabled(enabled);
    for (auto *t : m_audioTracks) t->setSnapEnabled(enabled);
    updateInfoLabel();
}

bool Timeline::snapEnabled() const {
    return m_videoTrack ? m_videoTrack->snapEnabled() : true;
}

// Zoom
void Timeline::zoomIn() { setZoomLevel(m_zoomLevel * 1.25); }
void Timeline::zoomOut() { setZoomLevel(m_zoomLevel / 1.25); }

void Timeline::setZoomLevel(double pixelsPerSecond)
{
    m_zoomLevel = qBound(0.1, pixelsPerSecond, 200.0);
    for (auto *t : m_videoTracks) t->setPixelsPerSecond(m_zoomLevel);
    for (auto *t : m_audioTracks) t->setPixelsPerSecond(m_zoomLevel);
    if (m_timeRuler)
        m_timeRuler->setPixelsPerSecond(m_zoomLevel);
    syncPlayheadOverlay();
    updateInfoLabel();
}

// I/O markers
void Timeline::markIn() { m_markIn = m_playheadPos; updateInfoLabel(); }
void Timeline::markOut() { m_markOut = m_playheadPos; updateInfoLabel(); }

// Clip speed
void Timeline::setClipSpeed(double speed)
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0) return;
    speed = qMax(0.25, qMin(4.0, speed));

    // Modify clips directly (need mutable access)
    auto videoClips = m_videoTrack->clips();
    auto audioClips = m_audioTrack->clips();
    videoClips[sel].speed = speed;
    if (sel < audioClips.size()) audioClips[sel].speed = speed;
    m_videoTrack->setClips(videoClips);
    m_audioTrack->setClips(audioClips);

    saveUndoState(QString("Set speed %1x").arg(speed));
    updateInfoLabel();
}

void Timeline::setClipVolume(double volume)
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0) return;
    volume = qMax(0.0, qMin(2.0, volume));
    auto audioClips = m_audioTrack->clips();
    if (sel < audioClips.size()) {
        audioClips[sel].volume = volume;
        m_audioTrack->setClips(audioClips);
    }
    saveUndoState(QString("Set volume %1%").arg(static_cast<int>(volume * 100)));
}

// --- Phase 3: Color correction, effects, keyframes ---

void Timeline::setClipColorCorrection(const ColorCorrection &cc)
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0) return;
    auto clips = m_videoTrack->clips();
    clips[sel].colorCorrection = cc;
    m_videoTrack->setClips(clips);
    saveUndoState("Color correction");
}

void Timeline::setClipEffects(const QVector<VideoEffect> &effects)
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0) return;
    auto clips = m_videoTrack->clips();
    clips[sel].effects = effects;
    m_videoTrack->setClips(clips);
    saveUndoState("Video effects");
}

void Timeline::setClipKeyframes(const KeyframeManager &km)
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0) return;
    auto clips = m_videoTrack->clips();
    clips[sel].keyframes = km;
    m_videoTrack->setClips(clips);
    saveUndoState("Keyframes");
}

ColorCorrection Timeline::clipColorCorrection() const
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0 || sel >= m_videoTrack->clips().size()) return {};
    return m_videoTrack->clips()[sel].colorCorrection;
}

QVector<VideoEffect> Timeline::clipEffects() const
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0 || sel >= m_videoTrack->clips().size()) return {};
    return m_videoTrack->clips()[sel].effects;
}

KeyframeManager Timeline::clipKeyframes() const
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0 || sel >= m_videoTrack->clips().size()) return {};
    return m_videoTrack->clips()[sel].keyframes;
}

double Timeline::selectedClipDuration() const
{
    int sel = m_videoTrack->selectedClip();
    if (sel < 0 || sel >= m_videoTrack->clips().size()) return 0.0;
    return m_videoTrack->clips()[sel].effectiveDuration();
}

void Timeline::addAudioFile(const QString &filePath)
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

    // Add to first audio track (or second if exists for BGM)
    TimelineTrack *target = m_audioTracks.size() > 1 ? m_audioTracks[1] : m_audioTrack;
    target->addClip(clip);
    saveUndoState("Add audio");
    updateInfoLabel();
}

void Timeline::toggleMuteTrack(int audioTrackIndex)
{
    if (audioTrackIndex < 0 || audioTrackIndex >= m_audioTracks.size()) return;
    auto *track = m_audioTracks[audioTrackIndex];
    track->setMuted(!track->isMuted());
    updateInfoLabel();
}

void Timeline::toggleSoloTrack(int audioTrackIndex)
{
    if (audioTrackIndex < 0 || audioTrackIndex >= m_audioTracks.size()) return;
    bool newSolo = !m_audioTracks[audioTrackIndex]->isSolo();
    // Clear all solo first, then set the target
    for (auto *t : m_audioTracks) t->setSolo(false);
    if (newSolo) m_audioTracks[audioTrackIndex]->setSolo(true);
    updateInfoLabel();
}

void Timeline::setPlayheadPosition(double seconds)
{
    m_playheadPos = qMax(0.0, seconds);
    syncPlayheadOverlay();
}

double Timeline::totalDuration() const
{
    // Maximum end time across ALL video tracks. Each track lays its clips out
    // sequentially from t=0, so each track's contribution is the sum of its
    // clip effective durations. Total sequence length is the longest track.
    double maxEnd = 0.0;
    for (auto *track : m_videoTracks) {
        if (!track) continue;
        double accum = 0.0;
        for (const auto &c : track->clips())
            accum += c.leadInSec + c.effectiveDuration();
        if (accum > maxEnd) maxEnd = accum;
    }
    return maxEnd;
}

void Timeline::onTrackClipClicked(int index)
{
    m_audioTrack->setSelectedClip(index);
    emit clipSelected(index);
}

bool Timeline::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton
            && !(me->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
            clearAllSelections();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Timeline::wireTrackSelection(TimelineTrack *track)
{
    connect(track, &TimelineTrack::selectionChanged, this, [this, track](int index, bool additive) {
        if (!additive && index >= 0) {
            auto clearOther = [track](TimelineTrack *t) {
                if (!t || t == track || t->selectedClip() < 0) return;
                const bool was = t->blockSignals(true);
                t->setSelectedClip(-1);
                t->blockSignals(was);
                t->update();
            };
            for (auto *t : m_videoTracks) clearOther(t);
            for (auto *t : m_audioTracks) clearOther(t);
        }
        emit clipSelected(index);
    });
    connect(track, &TimelineTrack::emptyAreaClicked, this, [this]() {
        clearAllSelections();
    });
    connect(track, &TimelineTrack::modified, this, &Timeline::onTrackModified);
}

void Timeline::clearAllSelections()
{
    bool changed = false;
    auto clearOne = [&changed](TimelineTrack *t) {
        if (!t || t->selectedClip() < 0) return;
        const bool was = t->blockSignals(true);
        t->setSelectedClip(-1);
        t->blockSignals(was);
        t->update();
        changed = true;
    };
    for (auto *t : m_videoTracks) clearOne(t);
    for (auto *t : m_audioTracks) clearOne(t);
    if (changed) emit clipSelected(-1);
}

void Timeline::onTrackModified()
{
    // Any clip add/remove/move/trim/split bubbles up here. Auto-fit the zoom
    // first so the timeline widget never exceeds the viewport width (otherwise
    // long-form sequences hit the kMaxWidth hard cap and tail clips get
    // visually clipped). Then recompute the flat playback schedule.
    ensureSequenceFitsViewport();
    emit sequenceChanged(computePlaybackSequence());
}

void Timeline::setTrackHeight(int h)
{
    h = qBound(30, h, 200);
    if (h == m_trackHeight) return;
    m_trackHeight = h;
    for (auto *t : m_videoTracks) if (t) t->setRowHeight(m_trackHeight);
    for (auto *t : m_audioTracks) if (t) t->setRowHeight(m_trackHeight);
    if (m_headerLayout) {
        for (int i = 0; i < m_headerLayout->count(); ++i) {
            if (auto *item = m_headerLayout->itemAt(i)) {
                if (auto *hw = item->widget())
                    hw->setFixedHeight(m_trackHeight);
            }
        }
    }
    updateInfoLabel();
}

void Timeline::increaseTrackHeight() { setTrackHeight(m_trackHeight + 10); }
void Timeline::decreaseTrackHeight() { setTrackHeight(m_trackHeight - 10); }

void Timeline::ensureSequenceFitsViewport()
{
    if (!m_scrollArea || !m_scrollArea->viewport())
        return;
    const double total = totalDuration();
    if (total <= 0.0)
        return;
    const int viewportW = m_scrollArea->viewport()->width();
    if (viewportW <= 100)
        return;

    // Leave a bit of right-edge margin so the last clip end is visible.
    const int safeW = qMax(100, viewportW - 60);

    // Required pps to make the entire sequence fit safeW horizontally.
    const double requiredPps = static_cast<double>(safeW) / total;
    const double newPps = qBound(0.1, requiredPps, 200.0);

    // Only auto-zoom OUT — never zoom in (that's the user's prerogative).
    if (newPps < m_zoomLevel - 1e-6) {
        qInfo() << "Timeline::ensureSequenceFitsViewport autoZoom"
                << m_zoomLevel << "->" << newPps
                << "totalSec=" << total << "viewportW=" << viewportW;
        setZoomLevel(newPps);
    }
}

QVector<PlaybackEntry> Timeline::computePlaybackSequence() const
{
    QVector<PlaybackEntry> result;
    if (m_videoTracks.isEmpty())
        return result;

    // V1-wins resolution (lower track index = higher priority). This is the
    // INVERSE of Premiere Pro semantics: V1 (the topmost displayed track) is
    // the primary playback layer; V2 only plays in time ranges where V1 has a
    // gap; V3 only fills gaps in V1+V2; and so on.
    //
    // Algorithm:
    //   1. Build per-track intervals laid out sequentially from t=0.
    //   2. Walk tracks from V1 upward. For each track, subtract every range
    //      already in `visible` (covered by higher-priority tracks) from the
    //      track's clips, then append surviving fragments.
    //   3. Sort by timelineStart and emit.

    struct Interval {
        double timelineStart;
        double timelineEnd;
        double clipIn;
        double clipOut;
        double speed;
        QString filePath;
        int trackIdx;
    };

    QVector<QVector<Interval>> trackIntervals;
    trackIntervals.reserve(m_videoTracks.size());
    for (int t = 0; t < m_videoTracks.size(); ++t) {
        QVector<Interval> ivs;
        auto *track = m_videoTracks[t];
        // Skip hidden video tracks entirely — they contribute nothing to playback.
        if (track && !track->isHidden()) {
            const auto &clips = track->clips();
            double accum = 0.0;
            for (const auto &c : clips) {
                accum += qMax(0.0, c.leadInSec); // leading gap before this clip
                const double clipDur = c.effectiveDuration();
                if (clipDur <= 0.0) continue;
                Interval iv;
                iv.timelineStart = accum;
                iv.timelineEnd = accum + clipDur;
                iv.clipIn = c.inPoint;
                iv.clipOut = (c.outPoint > 0.0) ? c.outPoint : c.duration;
                iv.speed = (c.speed > 0.0) ? c.speed : 1.0;
                iv.filePath = c.filePath;
                iv.trackIdx = t;
                ivs.append(iv);
                accum += clipDur;
            }
        }
        trackIntervals.append(ivs);
    }

    auto subtractRange = [](const QVector<Interval> &input,
                            double cutStart, double cutEnd) -> QVector<Interval> {
        QVector<Interval> out;
        out.reserve(input.size());
        for (const auto &iv : input) {
            if (iv.timelineEnd <= cutStart || iv.timelineStart >= cutEnd) {
                out.append(iv);
                continue;
            }
            if (iv.timelineStart < cutStart) {
                Interval left = iv;
                left.timelineEnd = cutStart;
                left.clipOut = iv.clipIn + (cutStart - iv.timelineStart) * iv.speed;
                out.append(left);
            }
            if (iv.timelineEnd > cutEnd) {
                Interval right = iv;
                right.timelineStart = cutEnd;
                right.clipIn = iv.clipIn + (cutEnd - iv.timelineStart) * iv.speed;
                out.append(right);
            }
        }
        return out;
    };

    QVector<Interval> visible;
    for (int t = 0; t < trackIntervals.size(); ++t) {
        QVector<Interval> trackClips = trackIntervals[t];
        // Subtract every range already covered by higher-priority tracks
        // (lower track index). V1 always wins, V2 fills V1's gaps, etc.
        for (const auto &existing : visible) {
            trackClips = subtractRange(trackClips, existing.timelineStart, existing.timelineEnd);
        }
        for (const auto &iv : trackClips)
            visible.append(iv);
    }

    std::sort(visible.begin(), visible.end(),
              [](const Interval &a, const Interval &b) {
                  return a.timelineStart < b.timelineStart;
              });

    result.reserve(visible.size());
    for (const auto &iv : visible) {
        if (iv.timelineEnd - iv.timelineStart < 1e-6) continue;
        PlaybackEntry e;
        e.filePath = iv.filePath;
        e.clipIn = iv.clipIn;
        e.clipOut = iv.clipOut;
        e.timelineStart = iv.timelineStart;
        e.timelineEnd = iv.timelineEnd;
        e.speed = iv.speed;
        e.sourceTrack = iv.trackIdx;
        qInfo() << "[SEQ] entry idx=" << result.size()
                << "tl=[" << iv.timelineStart << "," << iv.timelineEnd << "]"
                << "clip=[" << iv.clipIn << "," << iv.clipOut << "]"
                << "track=" << iv.trackIdx << "file=" << iv.filePath;
        // Audio routing: the corresponding A<n> track at the same index acts as
        // the audio mute switch for this entry's media file.
        if (iv.trackIdx >= 0 && iv.trackIdx < m_audioTracks.size()
            && m_audioTracks[iv.trackIdx] && m_audioTracks[iv.trackIdx]->isMuted()) {
            e.audioMuted = true;
        }
        result.append(e);
    }
    return result;
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
    {
        const bool vWas = m_videoTrack->blockSignals(true);
        m_videoTrack->setSelectedClip(state.selectedClip);
        m_videoTrack->blockSignals(vWas);
        m_videoTrack->update();
        const bool aWas = m_audioTrack->blockSignals(true);
        m_audioTrack->setSelectedClip(state.selectedClip);
        m_audioTrack->blockSignals(aWas);
        m_audioTrack->update();
    }
    m_playheadPos = state.playheadPos;
    syncPlayheadOverlay();
    emit clipSelected(state.selectedClip);
    // setClips bypasses the modified() signal path; trigger explicitly so the
    // VideoPlayer rebuilds its sequence after undo/redo.
    emit sequenceChanged(computePlaybackSequence());
}

// --- Project save/load ---

QVector<QVector<ClipInfo>> Timeline::allVideoTracks() const
{
    QVector<QVector<ClipInfo>> result;
    for (const auto *t : m_videoTracks)
        result.append(t->clips());
    return result;
}

QVector<QVector<ClipInfo>> Timeline::allAudioTracks() const
{
    QVector<QVector<ClipInfo>> result;
    for (const auto *t : m_audioTracks)
        result.append(t->clips());
    return result;
}

void Timeline::restoreFromProject(const QVector<QVector<ClipInfo>> &videoTracks,
                                   const QVector<QVector<ClipInfo>> &audioTracks,
                                   double playhead, double markInVal, double markOutVal, int zoom)
{
    // Set first video/audio track clips
    if (!videoTracks.isEmpty())
        m_videoTrack->setClips(videoTracks[0]);
    if (!audioTracks.isEmpty())
        m_audioTrack->setClips(audioTracks[0]);

    // Add extra video tracks
    for (int i = 1; i < videoTracks.size(); ++i) {
        if (i >= m_videoTracks.size()) addVideoTrack();
        m_videoTracks[i]->setClips(videoTracks[i]);
    }

    // Add extra audio tracks
    for (int i = 1; i < audioTracks.size(); ++i) {
        if (i >= m_audioTracks.size()) addAudioTrack();
        m_audioTracks[i]->setClips(audioTracks[i]);
    }

    m_playheadPos = playhead;
    m_markIn = markInVal;
    m_markOut = markOutVal;
    setZoomLevel(zoom);
    syncPlayheadOverlay();
    saveUndoState("Load project");
    updateInfoLabel();
    // setClips bypasses modified(); trigger sequence rebuild explicitly.
    emit sequenceChanged(computePlaybackSequence());
}

void Timeline::syncPlayheadOverlay()
{
    if (!m_playheadOverlay || !m_videoTrack)
        return;

    const int playheadX = m_videoTrack->secondsToX(m_playheadPos);
    m_playheadOverlay->setPlayheadX(playheadX);

    if (m_scrollArea) {
        QScrollBar *hbar = m_scrollArea->horizontalScrollBar();
        if (hbar) {
            const int viewportW = m_scrollArea->viewport() ? m_scrollArea->viewport()->width() : 0;
            const int scrollX = hbar->value();
            const int margin = qMax(40, viewportW / 10);
            if (viewportW > 0 && (playheadX < scrollX + margin || playheadX > scrollX + viewportW - margin)) {
                int target = playheadX - viewportW / 2;
                target = qBound(hbar->minimum(), target, hbar->maximum());
                hbar->setValue(target);
            }
        }
    }
}

void Timeline::updateInfoLabel()
{
    QString info = QString("Timeline - %1 clip(s) | Zoom %2 pps | %3")
        .arg(m_videoTrack->clipCount())
        .arg(m_zoomLevel, 0, 'f', 2)
        .arg(snapEnabled() ? "Snap ON" : "Snap OFF");
    if (m_videoTracks.size() > 1 || m_audioTracks.size() > 1)
        info += QString(" | V%1 A%2").arg(m_videoTracks.size()).arg(m_audioTracks.size());
    if (hasMarkedRange())
        info += QString(" | I/O: %1s-%2s").arg(m_markIn, 0, 'f', 1).arg(m_markOut, 0, 'f', 1);
    m_infoLabel->setText(info);
}
