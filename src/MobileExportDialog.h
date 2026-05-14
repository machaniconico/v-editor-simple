#pragma once
#include <QDialog>
#include <QSize>
#include "ExportDialog.h"
#include "MobileDeviceProfile.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class MobileExportDialog : public QDialog {
    Q_OBJECT
public:
    // sourceSize     : dimensions of the clip being exported
    // measuredLufs   : integrated loudness of the source audio
    explicit MobileExportDialog(const QSize& sourceSize,
                                double       measuredLufs,
                                QWidget*     parent = nullptr);

    // Returns the fully resolved ExportConfig for the current selection.
    ExportConfig resolvedConfig() const;

signals:
    void exportRequested(const ExportConfig& config);

private slots:
    void onCategoryChanged(int index);
    void onDeviceChanged(int index);
    void onBrowse();
    void onExport();

private:
    void setupUI();
    void populateDeviceCombo(mobile::Category category);
    void updateSummary();

    QSize  m_sourceSize;
    double m_measuredLufs;

    QComboBox*  m_categoryCombo = nullptr;
    QComboBox*  m_deviceCombo   = nullptr;
    QLabel*     m_summaryLabel  = nullptr;
    QLabel*     m_rotateLabel   = nullptr;
    QLineEdit*  m_outputEdit    = nullptr;
    QPushButton* m_browseBtn    = nullptr;
    QPushButton* m_exportBtn    = nullptr;
};
