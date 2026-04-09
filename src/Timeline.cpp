#include "Timeline.h"

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

    // Draw playhead line
    painter.setPen(QPen(QColor(0xFF, 0x44, 0x44), 2));
    painter.drawLine(m_playheadX, 0, m_playheadX, height());

    // Draw playhead handle (triangle at top)
    QPolygon triangle;
    triangle << QPoint(m_playheadX - 6, 0)
             << QPoint(m_playheadX + 6, 0)
             << QPoint(m_playheadX, 10);
    painter.setBrush(QColor(0xFF, 0x44, 0x44));
    painter.drawPolygon(triangle);
}

void PlayheadOverlay::mousePressEvent(QMouseEvent *event)
{
    if (qAbs(event->pos().x() - m_playheadX) < 10 || event->pos().y() < 15) {
        m_dragging = true;
        m_playheadX = event->pos().x();
        emit playheadMoved(m_playheadX);
        update();
    } else {
        // Click anywhere on ruler area to move playhead
        m_playheadX = event->pos().x();
        emit playheadMoved(m_playheadX);
        update();
    }
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
}

void TimelineTrack::addClip(const ClipInfo &clip)
{
    m_clips.append(clip);
    updateMinimumWidth();
    update();
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
}

void TimelineTrack::splitClipAt(int index, double localSeconds)
{
    if (index < 0 || index >= m_clips.size()) return;

    ClipInfo &original = m_clips[index];
    double effectiveStart = original.inPoint;
    double effectiveEnd = (original.outPoint > 0.0) ? original.outPoint : original.duration;
    double splitPoint = effectiveStart + localSeconds;

    if (splitPoint <= effectiveStart + 0.1 || splitPoint >= effectiveEnd - 0.1)
        return; // Too close to edges

    ClipInfo secondHalf = original;
    secondHalf.inPoint = splitPoint;
    secondHalf.outPoint = effectiveEnd;

    original.outPoint = splitPoint;

    m_clips.insert(index + 1, secondHalf);
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
        if (x >= cx && x < cx + clipWidth)
            return i;
        cx += clipWidth;
    }
    return -1;
}

double TimelineTrack::xToSeconds(int x) const
{
    return static_cast<double>(x) / PIXELS_PER_SECOND;
}

int TimelineTrack::secondsToX(double seconds) const
{
    return static_cast<int>(seconds * PIXELS_PER_SECOND);
}

void TimelineTrack::updateMinimumWidth()
{
    int totalWidth = 0;
    for (const auto &c : m_clips)
        totalWidth += qMax(20, static_cast<int>(c.effectiveDuration() * PIXELS_PER_SECOND));
    setMinimumWidth(totalWidth);
}

void TimelineTrack::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int x = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        int clipWidth = qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
        QRect clipRect(x, 0, clipWidth, CLIP_HEIGHT);

        // Clip color
        QColor color = (i % 2 == 0) ? QColor(0x44, 0x88, 0xCC) : QColor(0x44, 0xAA, 0x88);
        if (i == m_selectedClip)
            color = color.lighter(140);
        painter.fillRect(clipRect, color);

        // Selection border
        if (i == m_selectedClip) {
            painter.setPen(QPen(Qt::yellow, 2));
            painter.drawRect(clipRect.adjusted(1, 1, -1, -1));
        } else {
            painter.setPen(QPen(Qt::white, 1));
            painter.drawRect(clipRect);
        }

        // Trim handles
        if (i == m_selectedClip) {
            painter.fillRect(QRect(x, 0, TRIM_HANDLE_WIDTH, CLIP_HEIGHT), QColor(255, 255, 255, 80));
            painter.fillRect(QRect(x + clipWidth - TRIM_HANDLE_WIDTH, 0, TRIM_HANDLE_WIDTH, CLIP_HEIGHT), QColor(255, 255, 255, 80));
        }

        // Clip label
        painter.setPen(Qt::white);
        QRect textRect = clipRect.adjusted(8, 4, -8, -4);
        QString label = m_clips[i].displayName;
        double dur = m_clips[i].effectiveDuration();
        int mins = static_cast<int>(dur) / 60;
        int secs = static_cast<int>(dur) % 60;
        label += QString(" [%1:%2]").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
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

        // Check if clicking on trim handles
        int cx = 0;
        for (int i = 0; i < clickedClip; ++i)
            cx += qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
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
        }
    } else if (clickedClip < 0) {
        setSelectedClip(-1);
    }
}

void TimelineTrack::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode == DragMode::None || m_dragClipIndex < 0) {
        // Update cursor for trim handles
        int hoverClip = clipAtX(event->pos().x());
        if (hoverClip >= 0 && hoverClip == m_selectedClip) {
            int cx = 0;
            for (int i = 0; i < hoverClip; ++i)
                cx += qMax(20, static_cast<int>(m_clips[i].effectiveDuration() * PIXELS_PER_SECOND));
            int clipWidth = qMax(20, static_cast<int>(m_clips[hoverClip].effectiveDuration() * PIXELS_PER_SECOND));
            int localX = event->pos().x() - cx;
            if (localX <= TRIM_HANDLE_WIDTH || localX >= clipWidth - TRIM_HANDLE_WIDTH)
                setCursor(Qt::SizeHorCursor);
            else
                setCursor(Qt::ArrowCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        return;
    }

    int dx = event->pos().x() - m_dragStartX;
    double deltaSec = static_cast<double>(dx) / PIXELS_PER_SECOND;
    ClipInfo &clip = m_clips[m_dragClipIndex];

    if (m_dragMode == DragMode::TrimLeft) {
        double newIn = qMax(0.0, m_dragOriginalValue + deltaSec);
        double maxIn = (clip.outPoint > 0 ? clip.outPoint : clip.duration) - 0.1;
        clip.inPoint = qMin(newIn, maxIn);
    } else if (m_dragMode == DragMode::TrimRight) {
        double newOut = qMin(clip.duration, m_dragOriginalValue + deltaSec);
        double minOut = clip.inPoint + 0.1;
        clip.outPoint = qMax(newOut, minOut);
    }

    updateMinimumWidth();
    update();
}

void TimelineTrack::mouseReleaseEvent(QMouseEvent *)
{
    m_dragMode = DragMode::None;
    m_dragClipIndex = -1;
}

// --- Timeline ---

Timeline::Timeline(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
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

    // Playhead overlay sits on top
    auto *playheadOverlay = new PlayheadOverlay(tracksContainer);
    playheadOverlay->setFixedHeight(15);
    playheadOverlay->setStyleSheet("background-color: #222;");
    connect(playheadOverlay, &PlayheadOverlay::playheadMoved, this, [this, playheadOverlay](int x) {
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
}

void Timeline::addClip(const QString &filePath)
{
    AVFormatContext *fmt = nullptr;
    double duration = 0.0;

    if (avformat_open_input(&fmt, filePath.toUtf8().constData(), nullptr, nullptr) == 0) {
        if (avformat_find_stream_info(fmt, nullptr) >= 0) {
            duration = static_cast<double>(fmt->duration) / AV_TIME_BASE;
        }
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

    m_infoLabel->setText(QString("Timeline - %1 clip(s)").arg(m_videoTrack->clipCount()));
}

void Timeline::splitAtPlayhead()
{
    // Find which clip the playhead is over
    double accum = 0.0;
    const auto &clips = m_videoTrack->clips();
    for (int i = 0; i < clips.size(); ++i) {
        double clipDur = clips[i].effectiveDuration();
        if (m_playheadPos >= accum && m_playheadPos < accum + clipDur) {
            double localPos = m_playheadPos - accum;
            m_videoTrack->splitClipAt(i, localPos);
            m_audioTrack->splitClipAt(i, localPos);
            m_infoLabel->setText(QString("Timeline - %1 clip(s)").arg(m_videoTrack->clipCount()));
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
    m_infoLabel->setText(QString("Timeline - %1 clip(s)").arg(m_videoTrack->clipCount()));
}

bool Timeline::hasSelection() const
{
    return m_videoTrack->selectedClip() >= 0;
}

void Timeline::setPlayheadPosition(double seconds)
{
    m_playheadPos = seconds;
    // Update overlay would go here
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
