#pragma once

#include <QDialog>
#include <QImage>
#include "ChromaKeyRefine.h"

class QLabel;
class QSlider;
class QPushButton;

class ChromaKeyRefineDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChromaKeyRefineDialog(QWidget *parent = nullptr);

    // Convenience: pre-load a source image from outside
    void setSourceImage(const QImage &img);

    // Access current config / result after dialog accepted
    chromakey::KeyConfig config() const { return m_cfg; }
    QImage               resultImage() const { return m_result; }

private slots:
    void onBrowseClicked();
    void onPickKeyColor();
    void onParamChanged();

private:
    void updateAfterView();
    void showSourceInLabel(const QImage &img);
    QImage makeCheckerboard(int w, int h, int tileSize = 16) const;
    QImage compositeOverBg(const QImage &matte) const;

    // Previews
    QLabel       *m_beforeView  = nullptr;
    QLabel       *m_afterView   = nullptr;

    // Controls
    QPushButton  *m_browseBtn   = nullptr;
    QPushButton  *m_keyColorBtn = nullptr;
    QSlider      *m_simSlider   = nullptr;  // 0-100 → similarity 0.0-1.0
    QSlider      *m_smoothSlider = nullptr; // 0-100 → smoothness 0.0-1.0
    QSlider      *m_spillSlider  = nullptr; // 0-100 → spillSuppress 0.0-1.0

    // State
    QImage                m_source;
    QImage                m_result;
    chromakey::KeyConfig  m_cfg;
};
