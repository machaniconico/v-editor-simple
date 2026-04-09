#include "Timeline.h"

extern "C" {
#include <libavformat/avformat.h>
}

// --- TimelineTrack ---

TimelineTrack::TimelineTrack(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(CLIP_HEIGHT);
    setMaximumHeight(CLIP_HEIGHT);
}

void TimelineTrack::addClip(const ClipInfo &clip)
{
    m_clips.append(clip);
    int totalWidth = 0;
    for (const auto &c : m_clips)
        totalWidth += static_cast<int>(c.duration * PIXELS_PER_SECOND);
    setMinimumWidth(totalWidth);
    update();
}

void TimelineTrack::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int x = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        int clipWidth = static_cast<int>(m_clips[i].duration * PIXELS_PER_SECOND);
        if (clipWidth < 20) clipWidth = 20;

        QRect clipRect(x, 0, clipWidth, CLIP_HEIGHT);

        QColor color = (i % 2 == 0) ? QColor(0x44, 0x88, 0xCC) : QColor(0x44, 0xAA, 0x88);
        painter.fillRect(clipRect, color);
        painter.setPen(Qt::white);
        painter.drawRect(clipRect);

        QRect textRect = clipRect.adjusted(4, 4, -4, -4);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
            painter.fontMetrics().elidedText(m_clips[i].displayName, Qt::ElideRight, textRect.width()));

        x += clipWidth;
    }
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

    m_scrollArea->setWidget(tracksWidget);
    layout->addWidget(m_scrollArea);

    setStyleSheet("background-color: #333;");
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

    m_videoTrack->addClip(clip);
    m_audioTrack->addClip(clip);

    m_infoLabel->setText(QString("Timeline - %1 clip(s)").arg(m_videoTrack->clips().size()));
}
