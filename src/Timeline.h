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

struct ClipInfo {
    QString filePath;
    QString displayName;
    double duration;
    double inPoint = 0.0;
    double outPoint = 0.0; // 0 means use full duration
    double effectiveDuration() const {
        double out = (outPoint > 0.0) ? outPoint : duration;
        return out - inPoint;
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
    void removeClip(int index);
    void splitClipAt(int index, double localSeconds);
    int clipCount() const { return m_clips.size(); }
    const QVector<ClipInfo> &clips() const { return m_clips; }

    void setSelectedClip(int index);
    int selectedClip() const { return m_selectedClip; }

    int clipAtX(int x) const;
    double xToSeconds(int x) const;
    int secondsToX(double seconds) const;

signals:
    void clipClicked(int index);
    void selectionChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateMinimumWidth();

    QVector<ClipInfo> m_clips;
    int m_selectedClip = -1;
    DragMode m_dragMode = DragMode::None;
    int m_dragClipIndex = -1;
    int m_dragStartX = 0;
    double m_dragOriginalValue = 0.0;

    static constexpr int CLIP_HEIGHT = 50;
    static constexpr int PIXELS_PER_SECOND = 10;
    static constexpr int TRIM_HANDLE_WIDTH = 6;
};

class Timeline : public QWidget
{
    Q_OBJECT

public:
    explicit Timeline(QWidget *parent = nullptr);

    void addClip(const QString &filePath);
    void splitAtPlayhead();
    void deleteSelectedClip();
    bool hasSelection() const;

    void setPlayheadPosition(double seconds);
    double playheadPosition() const { return m_playheadPos; }
    double totalDuration() const;
    const QVector<ClipInfo> &videoClips() const { return m_videoTrack->clips(); }

signals:
    void clipSelected(int index);
    void positionChanged(double seconds);

private slots:
    void onTrackClipClicked(int index);

private:
    void setupUI();

    TimelineTrack *m_videoTrack;
    TimelineTrack *m_audioTrack;
    QScrollArea *m_scrollArea;
    QLabel *m_infoLabel;
    double m_playheadPos = 0.0;
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

private:
    int m_playheadX = 0;
    bool m_dragging = false;
};
