#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QFileDialog>

class VideoPlayer;
class Timeline;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openFile();
    void exportVideo();
    void about();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupUI();

    VideoPlayer *m_player;
    Timeline *m_timeline;
    QStringList m_supportedFormats;
};
