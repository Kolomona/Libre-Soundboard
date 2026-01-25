#include "ShortcutsManager.h"
#include <QSettings>
#include <QDebug>

ShortcutsManager::ShortcutsManager()
    : QObject(nullptr)
{
    m_settings = new QSettings("libresoundboard", "libresoundboard", this);
    loadFromSettings();
}

ShortcutsManager::~ShortcutsManager()
{
    // QSettings will be deleted automatically as child object
}

ShortcutsManager& ShortcutsManager::instance()
{
    static ShortcutsManager inst;
    return inst;
}

QKeySequence ShortcutsManager::slotShortcut(int slotIndex) const
{
    return m_shortcuts.value(slotIndex, QKeySequence());
}

bool ShortcutsManager::setSlotShortcut(int slotIndex, const QKeySequence& sequence)
{
    // Empty sequences are allowed (to clear)
    if (sequence.isEmpty()) {
        clearSlotShortcut(slotIndex);
        return true;
    }

    // Check if this sequence is already assigned to a different slot
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it.key() != slotIndex && it.value() == sequence) {
            // Duplicate found
            return false;
        }
    }

    m_shortcuts[slotIndex] = sequence;
    saveToSettings();
    emit shortcutsChanged();
    return true;
}

void ShortcutsManager::clearSlotShortcut(int slotIndex)
{
    if (m_shortcuts.remove(slotIndex) > 0) {
        saveToSettings();
        emit shortcutsChanged();
    }
}

int ShortcutsManager::slotForShortcut(const QKeySequence& sequence) const
{
    if (sequence.isEmpty()) {
        return -1;
    }

    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it.value() == sequence) {
            return it.key();
        }
    }
    return -1;
}

bool ShortcutsManager::isShortcutAssigned(const QKeySequence& sequence) const
{
    return slotForShortcut(sequence) != -1;
}

QMap<int, QKeySequence> ShortcutsManager::allShortcuts() const
{
    return m_shortcuts;
}

void ShortcutsManager::clearAll()
{
    if (!m_shortcuts.isEmpty()) {
        m_shortcuts.clear();
        saveToSettings();
        emit shortcutsChanged();
    }
}

void ShortcutsManager::loadFromSettings()
{
    m_shortcuts.clear();
    
    // Check if shortcuts group exists at all
    m_settings->beginGroup("shortcuts");
    bool groupExists = m_settings->contains("_initialized") || !m_settings->allKeys().isEmpty();
    QStringList keys = m_settings->allKeys();
    m_settings->endGroup();
    
    // If group doesn't exist, this is first run - load defaults
    if (!groupExists) {
        loadDefaults();
        return;
    }
    
    // Load shortcuts from settings
    m_settings->beginGroup("shortcuts");
    for (const QString& key : keys) {
        if (key.startsWith("slot_")) {
            bool ok = false;
            int slotIndex = key.mid(5).toInt(&ok); // Remove "slot_" prefix
            if (ok) {
                QString seq = m_settings->value(key).toString();
                if (!seq.isEmpty()) {
                    m_shortcuts[slotIndex] = QKeySequence::fromString(seq);
                }
            }
        }
    }
    m_settings->endGroup();
}

void ShortcutsManager::saveToSettings()
{
    // Clear existing shortcuts group
    m_settings->beginGroup("shortcuts");
    m_settings->remove(""); // Remove all keys in this group
    m_settings->endGroup();
    
    // Write current shortcuts
    m_settings->beginGroup("shortcuts");
    // Write initialization marker
    m_settings->setValue("_initialized", true);
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        QString key = QString("slot_%1").arg(it.key());
        m_settings->setValue(key, it.value().toString());
    }
    m_settings->endGroup();
    
    m_settings->sync();
}

void ShortcutsManager::loadDefaults()
{
    // Legacy shortcuts: keys 1-9 map to slots 0-8, key 0 maps to slot 9
    m_shortcuts.clear();
    
    for (int i = 0; i < 9; ++i) {
        // Keys 1-9 map to slots 0-8
        m_shortcuts[i] = QKeySequence(Qt::Key_1 + i);
    }
    // Key 0 maps to slot 9
    m_shortcuts[9] = QKeySequence(Qt::Key_0);
    
    // Save defaults to settings
    saveToSettings();
    emit shortcutsChanged();
}
