#pragma once
#include "PreferencesPage.h"
#include <functional>

class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLineEdit;

class PrefAudioEnginePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefAudioEnginePage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;
private:
    QLineEdit* m_jackName = nullptr;
    QCheckBox* m_rememberConnections = nullptr;
};

class PrefGridLayoutPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefGridLayoutPage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;

signals:
    void dimensionsChanged(int rows, int cols);

private:
    QSpinBox* m_rows = nullptr;
    QSpinBox* m_cols = nullptr;
};

class PrefWaveformCachePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefWaveformCachePage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;
private:
    QSpinBox* m_size = nullptr;
    QSpinBox* m_ttl = nullptr;
    QLineEdit* m_cacheDir = nullptr;
};

class PrefFileHandlingPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefFileHandlingPage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;
private:
    QLineEdit* m_soundDir = nullptr;
};

class QTableWidget;

class PrefKeyboardShortcutsPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefKeyboardShortcutsPage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;
private:
    QTableWidget* m_table = nullptr;
};

class PrefStartupPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefStartupPage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};

class PrefDebugPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefDebugPage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;
private:
    QComboBox* m_level = nullptr;
};

class PrefKeepAlivePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefKeepAlivePage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;

    // Set callback for play test button to actually play sound
    // Parameters: overrideVolume, targetTab, targetSlot, isSpecificSlot, useSlotVolume
    using PlayTestCallback = std::function<void(float, int, int, bool, bool)>;
    void setPlayTestCallback(PlayTestCallback callback) { m_playTestCallback = callback; }

private:
    void updateSensitivityEnabled();
    void updateTargetControls();
    void updateVolumeControls();

    QCheckBox* m_enable = nullptr;
    QSpinBox* m_timeout = nullptr;
    QDoubleSpinBox* m_sensitivity = nullptr;
    QCheckBox* m_anyNonZero = nullptr;
    QComboBox* m_target = nullptr;
    QSpinBox* m_tabIndex = nullptr;
    QSpinBox* m_slotIndex = nullptr;
    QCheckBox* m_useSlotVolume = nullptr;
    QDoubleSpinBox* m_overrideVolume = nullptr;
    QCheckBox* m_autoConnect = nullptr;
    QPushButton* m_playTest = nullptr;
    PlayTestCallback m_playTestCallback;
};
