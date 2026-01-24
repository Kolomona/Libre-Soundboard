#pragma once
#include "PreferencesPage.h"

class QDoubleSpinBox;
class QSpinBox;
class QComboBox;

class PrefAudioEnginePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefAudioEnginePage(QWidget* parent = nullptr);
    void apply() override;
    void reset() override;
private:
    QDoubleSpinBox* m_gain = nullptr;
};

class PrefGridLayoutPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefGridLayoutPage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
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
};

class PrefFileHandlingPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefFileHandlingPage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};

class PrefKeyboardShortcutsPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefKeyboardShortcutsPage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
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
    explicit PrefKeepAlivePage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};
