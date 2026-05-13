#pragma once
#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QList>
#include <QHash>
#include <QPointer>

class QAction;

namespace shortcut {

enum class Preset {
    VEditor,
    Premiere,
    FinalCutPro,
    DaVinci
};

struct Binding {
    QString      actionId;
    QString      displayName;
    QString      category;
    QKeySequence sequence;
    QKeySequence defaultSequence;
};

class ShortcutManager : public QObject {
    Q_OBJECT
public:
    explicit ShortcutManager(QObject* parent = nullptr);

    void registerAction(QAction*       action,
                        const QString& actionId,
                        const QString& displayName,
                        const QString& category);

    QList<Binding> bindings() const;
    Binding        bindingFor(const QString& actionId) const;

    void setBinding(const QString& actionId, const QKeySequence& seq);

    Preset              currentPreset() const;
    void                applyPreset(Preset p);
    static QString      presetDisplayName(Preset p);
    static QList<Preset> availablePresets();

    void resetAllToDefaults();

    void loadFromSettings();
    void saveToSettings() const;

signals:
    void bindingsChanged();

private:
    void applyPresetInternal(Preset p);
    static QKeySequence                    presetSequence(Preset p, const QString& actionId);
    static QList<QPair<QString, QKeySequence>> presetBindingTable(Preset p);

    QList<Binding>                 m_bindings;
    QHash<QString, QPointer<QAction>> m_actions;
    Preset                         m_currentPreset = Preset::VEditor;
};

} // namespace shortcut
