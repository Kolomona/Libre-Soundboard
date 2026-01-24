#pragma once
#include "PreferencesPage.h"

class PrefAudioEnginePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefAudioEnginePage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};

class PrefGridLayoutPage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefGridLayoutPage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};

class PrefWaveformCachePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefWaveformCachePage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
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
    explicit PrefDebugPage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};

class PrefKeepAlivePage : public PreferencesPage {
    Q_OBJECT
public:
    explicit PrefKeepAlivePage(QWidget* parent = nullptr) : PreferencesPage(parent) {}
};
