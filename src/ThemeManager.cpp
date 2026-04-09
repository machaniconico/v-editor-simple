#include "ThemeManager.h"

ThemeManager &ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

void ThemeManager::applyTheme(ThemeType type, QWidget *root)
{
    m_current = type;
    QString qss;
    switch (type) {
    case ThemeType::Dark:     qss = darkTheme(); break;
    case ThemeType::Light:    qss = lightTheme(); break;
    case ThemeType::Midnight: qss = midnightTheme(); break;
    case ThemeType::Ocean:    qss = oceanTheme(); break;
    default: return;
    }
    root->setStyleSheet(qss);
}

void ThemeManager::applyCustomTheme(const QString &qss, QWidget *root)
{
    m_current = ThemeType::Custom;
    root->setStyleSheet(qss);
}

QVector<Theme> ThemeManager::availableThemes() const
{
    return {
        { ThemeType::Dark,     "Dark",     darkTheme() },
        { ThemeType::Light,    "Light",    lightTheme() },
        { ThemeType::Midnight, "Midnight", midnightTheme() },
        { ThemeType::Ocean,    "Ocean",    oceanTheme() },
    };
}

QString ThemeManager::themeName(ThemeType type)
{
    switch (type) {
    case ThemeType::Dark:     return "Dark";
    case ThemeType::Light:    return "Light";
    case ThemeType::Midnight: return "Midnight";
    case ThemeType::Ocean:    return "Ocean";
    case ThemeType::Custom:   return "Custom";
    }
    return "Unknown";
}

QString ThemeManager::darkTheme()
{
    return R"(
        QMainWindow, QWidget { background-color: #2b2b2b; color: #dcdcdc; }
        QMenuBar { background-color: #333; color: #dcdcdc; }
        QMenuBar::item:selected { background-color: #505050; }
        QMenu { background-color: #3c3c3c; color: #dcdcdc; border: 1px solid #555; }
        QMenu::item:selected { background-color: #4488CC; }
        QToolBar { background-color: #333; border: none; spacing: 4px; }
        QPushButton { background-color: #444; color: #dcdcdc; border: 1px solid #555;
                      padding: 4px 12px; border-radius: 3px; }
        QPushButton:hover { background-color: #505050; }
        QPushButton:pressed { background-color: #4488CC; }
        QSlider::groove:horizontal { background: #444; height: 6px; border-radius: 3px; }
        QSlider::handle:horizontal { background: #4488CC; width: 14px; margin: -4px 0;
                                     border-radius: 7px; }
        QScrollArea { background-color: #2a2a2a; }
        QLabel { color: #dcdcdc; }
        QStatusBar { background-color: #333; color: #aaa; }
        QComboBox { background-color: #444; color: #dcdcdc; border: 1px solid #555;
                    padding: 3px; border-radius: 3px; }
        QSpinBox, QDoubleSpinBox { background-color: #444; color: #dcdcdc;
                                    border: 1px solid #555; padding: 2px; }
        QLineEdit { background-color: #444; color: #dcdcdc; border: 1px solid #555;
                    padding: 3px; border-radius: 3px; }
        QListWidget { background-color: #3c3c3c; color: #dcdcdc; border: 1px solid #555; }
        QListWidget::item:selected { background-color: #4488CC; }
        QGroupBox { border: 1px solid #555; margin-top: 8px; padding-top: 8px; color: #aaa; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; }
        QCheckBox { color: #dcdcdc; }
        QSplitter::handle { background-color: #444; }
        QProgressBar { background-color: #444; border: 1px solid #555; border-radius: 3px;
                       text-align: center; color: #dcdcdc; }
        QProgressBar::chunk { background-color: #4488CC; border-radius: 2px; }
    )";
}

QString ThemeManager::lightTheme()
{
    return R"(
        QMainWindow, QWidget { background-color: #f5f5f5; color: #333; }
        QMenuBar { background-color: #e8e8e8; color: #333; }
        QMenuBar::item:selected { background-color: #d0d0d0; }
        QMenu { background-color: #fff; color: #333; border: 1px solid #ccc; }
        QMenu::item:selected { background-color: #3399ff; color: #fff; }
        QToolBar { background-color: #e8e8e8; border-bottom: 1px solid #ccc; spacing: 4px; }
        QPushButton { background-color: #e0e0e0; color: #333; border: 1px solid #bbb;
                      padding: 4px 12px; border-radius: 3px; }
        QPushButton:hover { background-color: #d0d0d0; }
        QPushButton:pressed { background-color: #3399ff; color: #fff; }
        QSlider::groove:horizontal { background: #ccc; height: 6px; border-radius: 3px; }
        QSlider::handle:horizontal { background: #3399ff; width: 14px; margin: -4px 0;
                                     border-radius: 7px; }
        QScrollArea { background-color: #eee; }
        QLabel { color: #333; }
        QStatusBar { background-color: #e8e8e8; color: #666; }
        QComboBox { background-color: #fff; color: #333; border: 1px solid #bbb;
                    padding: 3px; border-radius: 3px; }
        QSpinBox, QDoubleSpinBox { background-color: #fff; color: #333;
                                    border: 1px solid #bbb; padding: 2px; }
        QLineEdit { background-color: #fff; color: #333; border: 1px solid #bbb;
                    padding: 3px; border-radius: 3px; }
        QListWidget { background-color: #fff; color: #333; border: 1px solid #ccc; }
        QListWidget::item:selected { background-color: #3399ff; color: #fff; }
        QGroupBox { border: 1px solid #ccc; margin-top: 8px; padding-top: 8px; color: #666; }
        QSplitter::handle { background-color: #ccc; }
    )";
}

QString ThemeManager::midnightTheme()
{
    return R"(
        QMainWindow, QWidget { background-color: #1a1a2e; color: #e0e0ff; }
        QMenuBar { background-color: #16213e; color: #e0e0ff; }
        QMenuBar::item:selected { background-color: #0f3460; }
        QMenu { background-color: #16213e; color: #e0e0ff; border: 1px solid #0f3460; }
        QMenu::item:selected { background-color: #533483; }
        QToolBar { background-color: #16213e; border: none; spacing: 4px; }
        QPushButton { background-color: #0f3460; color: #e0e0ff; border: 1px solid #533483;
                      padding: 4px 12px; border-radius: 3px; }
        QPushButton:hover { background-color: #533483; }
        QPushButton:pressed { background-color: #e94560; }
        QSlider::groove:horizontal { background: #0f3460; height: 6px; border-radius: 3px; }
        QSlider::handle:horizontal { background: #e94560; width: 14px; margin: -4px 0;
                                     border-radius: 7px; }
        QScrollArea { background-color: #1a1a2e; }
        QStatusBar { background-color: #16213e; color: #8888aa; }
        QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
            background-color: #0f3460; color: #e0e0ff; border: 1px solid #533483;
            padding: 3px; border-radius: 3px; }
        QListWidget { background-color: #16213e; color: #e0e0ff; border: 1px solid #0f3460; }
        QListWidget::item:selected { background-color: #533483; }
        QSplitter::handle { background-color: #0f3460; }
    )";
}

QString ThemeManager::oceanTheme()
{
    return R"(
        QMainWindow, QWidget { background-color: #1b2838; color: #c7d5e0; }
        QMenuBar { background-color: #171d25; color: #c7d5e0; }
        QMenuBar::item:selected { background-color: #2a475e; }
        QMenu { background-color: #1b2838; color: #c7d5e0; border: 1px solid #2a475e; }
        QMenu::item:selected { background-color: #66c0f4; color: #1b2838; }
        QToolBar { background-color: #171d25; border: none; spacing: 4px; }
        QPushButton { background-color: #2a475e; color: #c7d5e0; border: 1px solid #3d6f8e;
                      padding: 4px 12px; border-radius: 3px; }
        QPushButton:hover { background-color: #3d6f8e; }
        QPushButton:pressed { background-color: #66c0f4; color: #1b2838; }
        QSlider::groove:horizontal { background: #2a475e; height: 6px; border-radius: 3px; }
        QSlider::handle:horizontal { background: #66c0f4; width: 14px; margin: -4px 0;
                                     border-radius: 7px; }
        QScrollArea { background-color: #1b2838; }
        QStatusBar { background-color: #171d25; color: #8899aa; }
        QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
            background-color: #2a475e; color: #c7d5e0; border: 1px solid #3d6f8e;
            padding: 3px; border-radius: 3px; }
        QListWidget { background-color: #1b2838; color: #c7d5e0; border: 1px solid #2a475e; }
        QListWidget::item:selected { background-color: #66c0f4; color: #1b2838; }
        QSplitter::handle { background-color: #2a475e; }
    )";
}
