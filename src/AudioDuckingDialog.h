#pragma once
#include <QDialog>
#include "AudioDucking.h"

class QDoubleSpinBox;
class QCheckBox;
class QWidget;
class DuckingPreviewWidget;

class AudioDuckingDialog : public QDialog {
    Q_OBJECT
public:
    explicit AudioDuckingDialog(const DuckingParams& initial, QWidget* parent = nullptr);

    DuckingParams params() const;
    bool duckingEnabled() const;

private slots:
    void onParamsChanged();

private:
    DuckingParams m_params;
    QCheckBox*            m_enabledCheck  = nullptr;
    QDoubleSpinBox*       m_thresholdSpin = nullptr;
    QDoubleSpinBox*       m_reductionSpin = nullptr;
    QDoubleSpinBox*       m_attackSpin    = nullptr;
    QDoubleSpinBox*       m_releaseSpin   = nullptr;
    QDoubleSpinBox*       m_holdSpin      = nullptr;
    QDoubleSpinBox*       m_kneeSpin      = nullptr;
    DuckingPreviewWidget* m_preview       = nullptr;
};
