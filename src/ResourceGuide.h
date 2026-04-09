#pragma once

#include <QDialog>
#include <QVector>
#include <QString>
#include <QUrl>

// --- Resource Entry ---

struct ResourceSite {
    QString name;
    QString url;
    QString description;  // brief note (Japanese)
};

struct ResourceCategory {
    QString name;
    QString icon;  // emoji for display
    QVector<ResourceSite> sites;
};

// --- Resource Guide Dialog ---

class ResourceGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResourceGuideDialog(QWidget *parent = nullptr);

    static QVector<ResourceCategory> allCategories();

private:
    void setupUI();
    void openUrl(const QString &url);
};
