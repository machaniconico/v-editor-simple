#pragma once
#include "SubtitleTranslator.h"
#include "CaptionTrack.h"
#include <QDialog>
#include <QComboBox>
#include <QPlainTextEdit>

class SubtitleTranslatorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SubtitleTranslatorDialog(QWidget *parent = nullptr);

private slots:
    void onLoadSrtClicked();
    void onTranslateClicked();
    void onFinished(const caption::Track &translated);
    void onFailed(const QString &error);

private:
    subxlat::TranslatorClient *m_client;
    QComboBox                 *m_providerCombo;
    QComboBox                 *m_targetLangCombo;
    QPlainTextEdit            *m_preview;
    caption::Track             m_track;
};
