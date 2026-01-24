#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTemporaryDir>

#include "../src/PreferencesManager.h"
#include "../src/PreferencesDialog.h"

static void clearPrefs()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
}

TEST_CASE("PreferencesManager path validation", "[preferences][paths][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();

    // Non-existent directory should fail validation
    bool isValid = pm.validatePath("/this/does/not/exist/xyz");
    REQUIRE(isValid == false);

    // Create a temporary valid directory
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    QString tmpPath = tmpDir.path();

    // Valid existing directory should pass
    isValid = pm.validatePath(tmpPath);
    REQUIRE(isValid == true);
}

TEST_CASE("PreferencesManager default and cache directories", "[preferences][paths][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();

    // Default should be home directory initially
    QString defaultDir = pm.defaultSoundDirectory();
    REQUIRE(!defaultDir.isEmpty());
    REQUIRE(QDir(defaultDir).exists());

    // Cache directory should have a default
    QString cacheDir = pm.cacheDirectory();
    REQUIRE(!cacheDir.isEmpty());
}

TEST_CASE("PreferencesManager default directory setter/getter", "[preferences][paths][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();

    QTemporaryDir tmpDir1;
    REQUIRE(tmpDir1.isValid());
    QString path1 = tmpDir1.path();

    // Set and get default sound directory
    pm.setDefaultSoundDirectory(path1);
    REQUIRE(pm.defaultSoundDirectory() == path1);

    // Verify it persists in QSettings
    QSettings s("libresoundboard", "libresoundboard");
    REQUIRE(s.value("file/defaultSoundDirectory", "").toString() == path1);
}

TEST_CASE("PreferencesManager cache directory setter/getter", "[preferences][paths][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();

    QTemporaryDir tmpDir1;
    REQUIRE(tmpDir1.isValid());
    QString path1 = tmpDir1.path();

    // Set and get cache directory
    pm.setCacheDirectory(path1);
    REQUIRE(pm.cacheDirectory() == path1);

    // Verify it persists in QSettings
    QSettings s("libresoundboard", "libresoundboard");
    REQUIRE(s.value("cache/cacheDirectory", "").toString() == path1);
}

TEST_CASE("FileHandling preferences page UI", "[preferences][paths][dialog][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    
    // Select File Handling category (index 3)
    tree->setCurrentItem(tree->topLevelItem(3));

    // Find the default sound directory field
    auto editSoundDir = dlg.findChild<QLineEdit*>("editDefaultSoundDir");
    auto btnSoundDir = dlg.findChild<QPushButton*>("btnBrowseSoundDir");
    
    REQUIRE(editSoundDir != nullptr);
    REQUIRE(btnSoundDir != nullptr);

    // Should show default directory initially
    QString defaultDir = PreferencesManager::instance().defaultSoundDirectory();
    REQUIRE(editSoundDir->text() == defaultDir);
}

TEST_CASE("WaveformCache preferences page with cache directory", "[preferences][paths][dialog][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    
    // Select Waveform Cache category (index 2)
    tree->setCurrentItem(tree->topLevelItem(2));

    // Find cache directory field
    auto editCacheDir = dlg.findChild<QLineEdit*>("editCacheDir");
    auto btnCacheDir = dlg.findChild<QPushButton*>("btnBrowseCacheDir");
    
    REQUIRE(editCacheDir != nullptr);
    REQUIRE(btnCacheDir != nullptr);

    // Should show default cache directory initially
    QString cacheDir = PreferencesManager::instance().cacheDirectory();
    REQUIRE(editCacheDir->text() == cacheDir);
}

TEST_CASE("FileHandling preferences page apply and reset", "[preferences][paths][dialog][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    
    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    
    // Select File Handling category
    tree->setCurrentItem(tree->topLevelItem(3));

    auto editSoundDir = dlg.findChild<QLineEdit*>("editDefaultSoundDir");
    REQUIRE(editSoundDir != nullptr);

    // Manually change the path in the text field
    editSoundDir->setText(tmpDir.path());

    // Press save button
    auto btnSave = dlg.findChild<QPushButton*>();  // Will find first button
    // Instead, find by name
    auto saveBtns = dlg.findChildren<QPushButton*>();
    QPushButton* saveBtn = nullptr;
    for (auto btn : saveBtns) {
        if (btn->text() == "Save") {
            saveBtn = btn;
            break;
        }
    }
    REQUIRE(saveBtn != nullptr);
    saveBtn->click();

    // Verify path was saved
    REQUIRE(pm.defaultSoundDirectory() == tmpDir.path());
}

TEST_CASE("WaveformCache preferences page apply and reset", "[preferences][paths][dialog][phase4]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();
    QTemporaryDir tmpDir;
    REQUIRE(tmpDir.isValid());
    
    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    
    // Select Waveform Cache category
    tree->setCurrentItem(tree->topLevelItem(2));

    auto editCacheDir = dlg.findChild<QLineEdit*>("editCacheDir");
    REQUIRE(editCacheDir != nullptr);

    // Manually change the path in the text field
    editCacheDir->setText(tmpDir.path());

    // Find and click save button
    auto saveBtns = dlg.findChildren<QPushButton*>();
    QPushButton* saveBtn = nullptr;
    for (auto btn : saveBtns) {
        if (btn->text() == "Save") {
            saveBtn = btn;
            break;
        }
    }
    REQUIRE(saveBtn != nullptr);
    saveBtn->click();

    // Verify path was saved
    REQUIRE(pm.cacheDirectory() == tmpDir.path());
}
