#include "ShortcutCustomizeDialog.h"
#include "ShortcutManager.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ShortcutCustomizeDialog::ShortcutCustomizeDialog(shortcut::ShortcutManager* mgr,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_mgr(mgr)
{
    setObjectName(QStringLiteral("shortcutCustomizeDialog"));
    setWindowTitle(tr("ショートカット設定"));
    resize(640, 480);

    // --- Top row: preset + filter ---
    auto* topLayout = new QHBoxLayout;

    auto* presetLabel = new QLabel(tr("プリセット:"), this);
    m_presetCombo = new QComboBox(this);
    const auto presets = shortcut::ShortcutManager::availablePresets();
    for (shortcut::Preset p : presets)
        m_presetCombo->addItem(shortcut::ShortcutManager::presetDisplayName(p),
                               static_cast<int>(p));

    // Set current preset
    {
        int idx = m_presetCombo->findData(static_cast<int>(m_mgr->currentPreset()));
        if (idx >= 0)
            m_presetCombo->setCurrentIndex(idx);
    }

    auto* filterLabel = new QLabel(tr("検索:"), this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("アクション名でフィルタ…"));

    topLayout->addWidget(presetLabel);
    topLayout->addWidget(m_presetCombo);
    topLayout->addSpacing(16);
    topLayout->addWidget(filterLabel);
    topLayout->addWidget(m_filterEdit, 1);

    // --- Table ---
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("カテゴリ"), tr("アクション"),
                                        tr("ショートカット"), tr("ID")});
    m_table->setColumnHidden(3, true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // --- Bottom row: reset + close ---
    auto* bottomLayout = new QHBoxLayout;

    m_resetButton = new QPushButton(tr("デフォルトに戻す"), this);
    m_closeButton = new QPushButton(tr("閉じる"), this);

    bottomLayout->addWidget(m_resetButton);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_closeButton);

    // --- Root layout ---
    auto* root = new QVBoxLayout(this);
    root->addLayout(topLayout);
    root->addWidget(m_table, 1);
    root->addLayout(bottomLayout);
    setLayout(root);

    // --- Connections ---
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ShortcutCustomizeDialog::onPresetChanged);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &ShortcutCustomizeDialog::onFilterTextChanged);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &ShortcutCustomizeDialog::onItemDoubleClicked);
    connect(m_resetButton, &QPushButton::clicked,
            this, &ShortcutCustomizeDialog::onResetClicked);
    connect(m_closeButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(m_mgr, &shortcut::ShortcutManager::bindingsChanged,
            this, &ShortcutCustomizeDialog::rebuildTable);

    rebuildTable();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void ShortcutCustomizeDialog::rebuildTable()
{
    // Disable sorting temporarily to avoid index confusion during insertion
    m_table->setSortingEnabled(false);

    const QList<shortcut::Binding> bindings = m_mgr->bindings();
    m_table->setRowCount(bindings.size());

    for (int row = 0; row < bindings.size(); ++row) {
        const shortcut::Binding& b = bindings.at(row);

        auto* catItem  = new QTableWidgetItem(b.category);
        auto* nameItem = new QTableWidgetItem(b.displayName);
        auto* keyItem  = new QTableWidgetItem(b.sequence.toString(QKeySequence::NativeText));
        auto* idItem   = new QTableWidgetItem(b.actionId);

        catItem->setFlags(catItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);

        m_table->setItem(row, 0, catItem);
        m_table->setItem(row, 1, nameItem);
        m_table->setItem(row, 2, keyItem);
        m_table->setItem(row, 3, idItem);
    }

    m_table->setSortingEnabled(true);

    // Re-apply current filter
    const QString filter = m_currentFilter;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* nameItem = m_table->item(row, 1);
        bool hidden = !filter.isEmpty()
                      && nameItem
                      && !nameItem->text().contains(filter, Qt::CaseInsensitive);
        m_table->setRowHidden(row, hidden);
    }
}

void ShortcutCustomizeDialog::refreshRow(int row)
{
    if (row < 0 || row >= m_table->rowCount())
        return;

    const QTableWidgetItem* idItem = m_table->item(row, 3);
    if (!idItem)
        return;

    const QString actionId = idItem->text();
    const shortcut::Binding b = m_mgr->bindingFor(actionId);

    QTableWidgetItem* keyItem = m_table->item(row, 2);
    if (keyItem)
        keyItem->setText(b.sequence.toString(QKeySequence::NativeText));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void ShortcutCustomizeDialog::onPresetChanged(int comboIndex)
{
    const QVariant data = m_presetCombo->itemData(comboIndex);
    if (!data.isValid())
        return;

    const auto preset = static_cast<shortcut::Preset>(data.toInt());

    // Disconnect bindingsChanged temporarily to avoid double-rebuild;
    // applyPreset emits bindingsChanged which calls rebuildTable via connection.
    m_mgr->applyPreset(preset);
    // rebuildTable() already called via signal connection
}

void ShortcutCustomizeDialog::onFilterTextChanged(const QString& filter)
{
    m_currentFilter = filter;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* nameItem = m_table->item(row, 1);
        bool hidden = !filter.isEmpty()
                      && nameItem
                      && !nameItem->text().contains(filter, Qt::CaseInsensitive);
        m_table->setRowHidden(row, hidden);
    }
}

void ShortcutCustomizeDialog::onItemDoubleClicked(int row, int column)
{
    if (column != 2)
        return;

    const QTableWidgetItem* idItem  = m_table->item(row, 3);
    const QTableWidgetItem* keyItem = m_table->item(row, 2);
    if (!idItem || !keyItem)
        return;

    const QString actionId     = idItem->text();
    const QString currentKeyStr = keyItem->text();

    bool ok = false;
    const QString newKeyStr = QInputDialog::getText(
        this,
        tr("ショートカットを変更"),
        tr("新しいショートカットを入力してください (例: Ctrl+Shift+O):"),
        QLineEdit::Normal,
        currentKeyStr,
        &ok);

    if (!ok || newKeyStr.trimmed().isEmpty())
        return;

    const QKeySequence newSeq(newKeyStr.trimmed());
    onRowEditKeyChanged(row, newSeq);
}

void ShortcutCustomizeDialog::onResetClicked()
{
    m_mgr->resetAllToDefaults();
    // rebuildTable() called via bindingsChanged signal
}

void ShortcutCustomizeDialog::onRowEditKeyChanged(int row, const QKeySequence& newSeq)
{
    const QTableWidgetItem* idItem = m_table->item(row, 3);
    if (!idItem)
        return;

    const QString actionId = idItem->text();
    m_mgr->setBinding(actionId, newSeq);
    refreshRow(row);
}
