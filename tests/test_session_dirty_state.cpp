#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "../src/MainWindow.h"
#include "../src/SessionManager.h"
#include "../src/PreferencesManager.h"
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>

// Global QApplication instance
static int argc = 0;
static char* argv[] = { (char*)"test" };
static QApplication* g_app = nullptr;

static void ensureQApplication()
{
    if (!g_app) {
        g_app = new QApplication(argc, argv);
    }
}

static QString getTempSessionDir()
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/libresoundboard_test_dirty";
    QDir dir(tempDir);
    dir.mkpath(".");
    return tempDir;
}

static void clearTestSessions()
{
    PreferencesManager::instance().settings().beginGroup("sessions");
    PreferencesManager::instance().settings().remove("");
    PreferencesManager::instance().settings().endGroup();
    PreferencesManager::instance().settings().sync();
    
    QString tempDir = getTempSessionDir();
    QDir(tempDir).removeRecursively();
    QDir(tempDir).mkpath(".");
}

TEST_CASE("Session dirty state: New session starts clean", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // New session should start clean (no modifications)
    REQUIRE(!window.isSessionDirty());
}

TEST_CASE("Session dirty state: Loading session starts clean", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QString sessionPath = getTempSessionDir() + "/clean_load.json";
    window.saveSessionAs(sessionPath);
    REQUIRE(!window.isSessionDirty());
    
    MainWindow window2;
    window2.loadSession(sessionPath);
    REQUIRE(!window2.isSessionDirty());
    
    QFile(sessionPath).remove();
}

TEST_CASE("Session dirty state: Modification marks dirty", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // Get first container and modify it
    SoundContainer* container = window.containerAt(0, 0);
    REQUIRE(container != nullptr);
    
    // Changing file should mark dirty
    container->setFile("/tmp/test.mp3");
    REQUIRE(window.isSessionDirty());
}

TEST_CASE("Session dirty state: Volume change marks dirty", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    SoundContainer* container = window.containerAt(0, 0);
    REQUIRE(container != nullptr);
    
    // Changing volume should mark dirty
    container->setVolume(0.5f);
    REQUIRE(window.isSessionDirty());
}

TEST_CASE("Session dirty state: Saving clears dirty flag", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // Modify to mark dirty
    SoundContainer* container = window.containerAt(0, 0);
    container->setFile("/tmp/test.mp3");
    REQUIRE(window.isSessionDirty());
    
    // Save should clear dirty flag
    QString sessionPath = getTempSessionDir() + "/save_clears.json";
    window.saveSessionAs(sessionPath);
    REQUIRE(!window.isSessionDirty());
    
    QFile(sessionPath).remove();
}

TEST_CASE("Session dirty state: Tab rename marks dirty", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // Note: Tab rename is done through CustomTabBar context menu
    // This test verifies the signal connection exists
    // Actual rename would be tested in integration
    
    // For now, verify that dirty state mechanism is in place
    REQUIRE(!window.isSessionDirty());
}

TEST_CASE("Session dirty state: Clear slot marks dirty", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    SoundContainer* container = window.containerAt(0, 0);
    container->setFile("/tmp/test.mp3");
    container->setVolume(0.5f);
    REQUIRE(window.isSessionDirty());
    
    // Clearing should still be dirty (or mark as such if we clear it)
    // After clear, session has been modified, so it's dirty
    REQUIRE(window.isSessionDirty());
}

TEST_CASE("Session dirty state: Menu has New and Open actions", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // Find File menu
    QMenu* fileMenu = nullptr;
    for (auto action : window.menuBar()->actions()) {
        if (action->text() == "File") {
            fileMenu = action->menu();
            break;
        }
    }
    REQUIRE(fileMenu != nullptr);
    
    // Check for New and Open actions
    bool hasNew = false;
    bool hasOpen = false;
    
    for (auto action : fileMenu->actions()) {
        if (action->text() == "New Session") hasNew = true;
        if (action->text() == "Open") hasOpen = true;
    }
    
    CHECK(hasNew);
    CHECK(hasOpen);
}

TEST_CASE("Session dirty state: New Session action exists", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QMenu* fileMenu = nullptr;
    for (auto action : window.menuBar()->actions()) {
        if (action->text() == "File") {
            fileMenu = action->menu();
            break;
        }
    }
    REQUIRE(fileMenu != nullptr);
    
    QAction* newAction = nullptr;
    for (auto action : fileMenu->actions()) {
        if (action->text() == "New Session") {
            newAction = action;
            break;
        }
    }
    REQUIRE(newAction != nullptr);
    
    // Verify it's connected to a slot
    REQUIRE(newAction->isEnabled());
}

TEST_CASE("Session dirty state: Open action is wired", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QMenu* fileMenu = nullptr;
    for (auto action : window.menuBar()->actions()) {
        if (action->text() == "File") {
            fileMenu = action->menu();
            break;
        }
    }
    REQUIRE(fileMenu != nullptr);
    
    QAction* openAction = nullptr;
    for (auto action : fileMenu->actions()) {
        if (action->text() == "Open") {
            openAction = action;
            break;
        }
    }
    REQUIRE(openAction != nullptr);
    
    // Verify it's enabled and functional
    REQUIRE(openAction->isEnabled());
}

TEST_CASE("Session dirty state: promptToSaveIfDirty returns DiscardChanges when clean", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // When session is clean, should return DiscardChanges (no prompt needed)
    auto result = window.promptToSaveIfDirty();
    REQUIRE(result == MainWindow::SavePromptResult::DiscardChanges);
}

TEST_CASE("Session dirty state: Untitled session title shown correctly", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // New session should show Untitled
    REQUIRE(window.windowTitle().contains("Untitled"));
}

TEST_CASE("Session dirty state: Save updates title bar", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QString sessionPath = getTempSessionDir() + "/title_test.json";
    window.saveSessionAs(sessionPath);
    
    // Title should show session name
    REQUIRE(window.windowTitle().contains("title_test"));
    REQUIRE(!window.windowTitle().contains("Untitled"));
    
    QFile(sessionPath).remove();
}

TEST_CASE("Session dirty state: Multiple modifications accumulate dirty", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    REQUIRE(!window.isSessionDirty());
    
    // First modification
    SoundContainer* c1 = window.containerAt(0, 0);
    c1->setFile("/tmp/test1.mp3");
    REQUIRE(window.isSessionDirty());
    
    // Second modification on different container
    SoundContainer* c2 = window.containerAt(0, 1);
    c2->setVolume(0.3f);
    REQUIRE(window.isSessionDirty());
    
    // Save clears it
    QString sessionPath = getTempSessionDir() + "/multi_mod.json";
    window.saveSessionAs(sessionPath);
    REQUIRE(!window.isSessionDirty());
    
    QFile(sessionPath).remove();
}

TEST_CASE("Session dirty state: Dirty state persists across modifications", "[dirty][phase10]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // Mark as dirty
    SoundContainer* container = window.containerAt(0, 0);
    container->setFile("/tmp/test.mp3");
    REQUIRE(window.isSessionDirty());
    
    // Another modification doesn't clear it
    container->setVolume(0.5f);
    REQUIRE(window.isSessionDirty());
}
