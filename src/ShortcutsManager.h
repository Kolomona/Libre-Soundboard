#pragma once
#include <QObject>
#include <QKeySequence>
#include <QString>
#include <QMap>

class QSettings;

/**
 * @brief Manages keyboard shortcuts for slots and global actions
 * 
 * ShortcutsManager handles storage, retrieval, and validation of keyboard shortcuts
 * for sound container slots. It ensures no duplicate shortcuts are assigned and
 * provides a simple API for MainWindow to query shortcuts.
 */
class ShortcutsManager : public QObject {
    Q_OBJECT
public:
    static ShortcutsManager& instance();

    /**
     * @brief Get the keyboard shortcut for a specific slot
     * @param slotIndex The zero-based slot index
     * @return The key sequence, or empty sequence if no shortcut assigned
     */
    QKeySequence slotShortcut(int slotIndex) const;

    /**
     * @brief Set keyboard shortcut for a specific slot
     * @param slotIndex The zero-based slot index
     * @param sequence The key sequence to assign
     * @return true if successful, false if duplicate or invalid
     */
    bool setSlotShortcut(int slotIndex, const QKeySequence& sequence);

    /**
     * @brief Clear shortcut for a specific slot
     * @param slotIndex The zero-based slot index
     */
    void clearSlotShortcut(int slotIndex);

    /**
     * @brief Get slot index for a given key sequence
     * @param sequence The key sequence to look up
     * @return Slot index, or -1 if not found
     */
    int slotForShortcut(const QKeySequence& sequence) const;

    /**
     * @brief Check if a shortcut is already assigned
     * @param sequence The key sequence to check
     * @return true if assigned to any slot
     */
    bool isShortcutAssigned(const QKeySequence& sequence) const;

    /**
     * @brief Get all assigned shortcuts as a map
     * @return Map of slot index to key sequence
     */
    QMap<int, QKeySequence> allShortcuts() const;

    /**
     * @brief Clear all shortcuts
     */
    void clearAll();

    /**
     * @brief Load default shortcuts (legacy 1-9,0 for first 10 slots)
     */
    void loadDefaults();

signals:
    /**
     * @brief Emitted when shortcuts change
     */
    void shortcutsChanged();

private:
    ShortcutsManager();
    ~ShortcutsManager();
    ShortcutsManager(const ShortcutsManager&) = delete;
    ShortcutsManager& operator=(const ShortcutsManager&) = delete;

    void loadFromSettings();
    void saveToSettings();

    // Map from slot index to key sequence
    QMap<int, QKeySequence> m_shortcuts;
    QSettings* m_settings = nullptr;
};
