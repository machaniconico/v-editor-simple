#include "MainWindow.h"
#include "VideoPlayer.h"
#include "Timeline.h"
#include <QApplication>
#include <QMessageBox>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("V Editor Simple");
    resize(1280, 720);

    m_supportedFormats = {
        "MP4 (*.mp4)",
        "MKV (*.mkv)",
        "MOV (*.mov)",
        "WebM (*.webm)",
        "FLV (*.flv)"
    };

    setupUI();
    setupMenuBar();
    setupToolBar();

    statusBar()->showMessage("Ready");
}

void MainWindow::setupUI()
{
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_player = new VideoPlayer(this);
    m_timeline = new Timeline(this);

    splitter->addWidget(m_player);
    splitter->addWidget(m_timeline);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
    setCentralWidget(centralWidget);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");

    auto *openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    auto *exportAction = fileMenu->addAction("&Export...");
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportVideo);

    fileMenu->addSeparator();

    auto *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    auto *helpMenu = menuBar()->addMenu("&Help");
    auto *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::setupToolBar()
{
    auto *toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    toolbar->addAction("Open", this, &MainWindow::openFile);
    toolbar->addSeparator();
    toolbar->addAction("Export", this, &MainWindow::exportVideo);
}

void MainWindow::openFile()
{
    QString filter = "Video Files (" + m_supportedFormats.join(" ").replace("(", "").replace(")", "").replace("MP4 ", "").replace("MKV ", "").replace("MOV ", "").replace("WebM ", "").replace("FLV ", "") + ")";
    filter = "Video Files (*.mp4 *.mkv *.mov *.webm *.flv);;All Files (*)";

    QString filePath = QFileDialog::getOpenFileName(this, "Open Video", QString(), filter);
    if (!filePath.isEmpty()) {
        m_player->loadFile(filePath);
        m_timeline->addClip(filePath);
        statusBar()->showMessage("Loaded: " + filePath);
    }
}

void MainWindow::exportVideo()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Export Video", QString(), "MP4 (*.mp4);;MKV (*.mkv);;WebM (*.webm)");
    if (!filePath.isEmpty()) {
        statusBar()->showMessage("Exporting: " + filePath);
        // TODO: Implement export via FFmpeg
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, "About V Editor Simple",
        QString("V Editor Simple v%1\n\n"
                "A simple yet powerful video editor.\n"
                "Built with Qt and FFmpeg.\n\n"
                "Supported codecs: H.264, H.265, AV1\n"
                "Containers: MP4, MKV, MOV, WebM, FLV")
            .arg(APP_VERSION));
}
