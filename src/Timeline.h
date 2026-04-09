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

struct ClipInfo {
    QString filePath;
    QString displayName;
    double duration;
};

class TimelineTrack : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineTrack(QWidget *parent = nullptr);

    void addClip(const ClipInfo &clip);
    const QVector<ClipInfo> &clips() const { return m_clips; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<ClipInfo> m_clips;
    static constexpr int CLIP_HEIGHT = 50;
    static constexpr int PIXELS_PER_SECOND = 10;
};

class Timeline : public QWidget
{
    Q_OBJECT

public:
    explicit Timeline(QWidget *parent = nullptr);

    void addClip(const QString &filePath);

signals:
    void clipSelected(int index);
    void positionChanged(double seconds);

private:
    void setupUI();

    TimelineTrack *m_videoTrack;
    TimelineTrack *m_audioTrack;
    QScrollArea *m_scrollArea;
    QLabel *m_infoLabel;
};
