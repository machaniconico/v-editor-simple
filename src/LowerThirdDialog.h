#pragma once
#include <QDialog>
#include "LowerThirdTemplates.h"

class QListWidget;
class QLineEdit;
class QSlider;
class QLabel;

class LowerThirdDialog : public QDialog {
    Q_OBJECT
public:
    explicit LowerThirdDialog(QWidget *parent = nullptr);

private slots:
    void onStyleChanged(int row);
    void onTextEdited();
    void onProgressChanged(int value);

private:
    void buildUi();
    void updatePreview();

    QListWidget *m_styleList    = nullptr;
    QLineEdit   *m_primaryEdit  = nullptr;
    QLineEdit   *m_secondaryEdit = nullptr;
    QSlider     *m_progressSlider = nullptr;
    QLabel      *m_preview      = nullptr;

    lowerthird::LowerThirdStyle m_current;
};
