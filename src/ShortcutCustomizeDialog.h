#pragma once
#include <QDialog>
#include <QKeySequence>

namespace shortcut { class ShortcutManager; }

class QComboBox;
class QTableWidget;
class QLineEdit;
class QPushButton;

class ShortcutCustomizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutCustomizeDialog(shortcut::ShortcutManager* mgr,
                                     QWidget* parent = nullptr);

private slots:
    void onPresetChanged(int comboIndex);
    void onFilterTextChanged(const QString& filter);
    void onItemDoubleClicked(int row, int column);
    void onResetClicked();
    void onRowEditKeyChanged(int row, const QKeySequence& newSeq);

private:
    void rebuildTable();
    void refreshRow(int row);

    shortcut::ShortcutManager* m_mgr         = nullptr;
    QComboBox*                 m_presetCombo  = nullptr;
    QLineEdit*                 m_filterEdit   = nullptr;
    QTableWidget*              m_table        = nullptr;
    QPushButton*               m_resetButton  = nullptr;
    QPushButton*               m_closeButton  = nullptr;

    QString                    m_currentFilter;
};
