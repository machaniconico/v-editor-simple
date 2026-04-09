#pragma once

#include <QString>
#include <QWidget>
#include <QVector>

enum class ThemeType {
    Dark,
    Light,
    Midnight,
    Ocean,
    Custom
};

struct Theme {
    ThemeType type;
    QString name;
    QString stylesheet;
};

class ThemeManager
{
public:
    static ThemeManager &instance();

    void applyTheme(ThemeType type, QWidget *root);
    void applyCustomTheme(const QString &qss, QWidget *root);

    ThemeType currentTheme() const { return m_current; }
    QVector<Theme> availableThemes() const;
    static QString themeName(ThemeType type);

private:
    ThemeManager() = default;

    static QString darkTheme();
    static QString lightTheme();
    static QString midnightTheme();
    static QString oceanTheme();

    ThemeType m_current = ThemeType::Dark;
};
