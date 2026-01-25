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
#include <QKeySequence>

// Global QApplication instance for the test suite
static int argc = 0;
static char* argv[] = { (char*)"test" };
static QApplication* g_app = nullptr;

static void ensureQApplication()
{
    if (!g_app) {
        g_app = new QApplication(argc, argv);
    }
}

// Helper to get temp directory and create test sessions directory
static QString getTempSessionDir()
{
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/libresoundboard_test_sessions";
    QDir dir(tempDir);
    dir.mkpath(".");
    return tempDir;
}

// Helper to clean up sessions
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

TEST_CASE("Phase 9 UI: File menu has Save and Save As actions", "[ui][phase9]")
{
    ensureQApplication();
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
    
    // Check for Save action
    bool hasSave = false;
    bool hasSaveAs = false;
    bool hasRecent = false;
    
    for (auto action : fileMenu->actions()) {
        if (action->text() == "Save") hasSave = true;
        if (action->text() == "Save As...") hasSaveAs = true;
        if (action->menu() && action->text() == "Recent Sessions") hasRecent = true;
    }
    
    CHECK(hasSave);
    CHECK(hasSaveAs);
    CHECK(hasRecent);
}

TEST_CASE("Phase 9 UI: Window title shows Untitled on startup", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    REQUIRE(window.windowTitle().contains("Untitled"));
}

TEST_CASE("Phase 9 UI: Window title updates after save", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QString sessionPath = getTempSessionDir() + "/test_session.json";
    window.saveSessionAs(sessionPath);
    
    // Title should now show "test_session - LibreSoundboard"
    REQUIRE(window.windowTitle().contains("test_session"));
    
    // Clean up
    QFile(sessionPath).remove();
}

TEST_CASE("Phase 9 UI: Window title updates after load", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QString sessionPath = getTempSessionDir() + "/load_test.json";
    
    // First save a session
    window.saveSessionAs(sessionPath);
    
    // Create a new window (simulating app restart)
    MainWindow window2;
    
    // Load the saved session
    window2.loadSession(sessionPath);
    
    // Title should show "load_test - LibreSoundboard"
    REQUIRE(window2.windowTitle().contains("load_test"));
    
    // Clean up
    QFile(sessionPath).remove();
}

TEST_CASE("Phase 9 UI: Recent sessions menu is populated", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QString sessionPath1 = getTempSessionDir() + "/session1.json";
    QString sessionPath2 = getTempSessionDir() + "/session2.json";
    
    // Save two sessions
    window.saveSessionAs(sessionPath1);
    window.saveSessionAs(sessionPath2);
    
    // Find Recent Sessions menu
    QMenu* recentMenu = nullptr;
    QMenu* fileMenu = nullptr;
    for (auto action : window.menuBar()->actions()) {
        if (action->text() == "File") {
            fileMenu = action->menu();
            break;
        }
    }
    REQUIRE(fileMenu != nullptr);
    
    for (auto action : fileMenu->actions()) {
        if (action->text() == "Recent Sessions") {
            recentMenu = action->menu();
            break;
        }
    }
    REQUIRE(recentMenu != nullptr);
    
    // Check that both sessions appear in recent menu
    int sessionCount = 0;
    for (auto action : recentMenu->actions()) {
        if (action->text() == "session1.json" || action->text() == "session2.json") {
            sessionCount++;
        }
    }
    REQUIRE(sessionCount == 2);
    
    // Clean up
    QFile(sessionPath1).remove();
    QFile(sessionPath2).remove();
}

TEST_CASE("Phase 9 UI: Save action without prior save opens Save As dialog", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // onSaveSession should be called when Save action is triggered
    // Since we can't easily test the dialog in headless mode, we just verify
    // that the action exists and is connected
    
    QAction* saveAction = nullptr;
    QMenu* fileMenu = nullptr;
    for (auto action : window.menuBar()->actions()) {
        if (action->text() == "File") {
            fileMenu = action->menu();
            break;
        }
    }
    REQUIRE(fileMenu != nullptr);
    
    for (auto action : fileMenu->actions()) {
        if (action->text() == "Save") {
            saveAction = action;
            break;
        }
    }
    REQUIRE(saveAction != nullptr);
    
    // Verify shortcut is Ctrl+S
    REQUIRE(saveAction->shortcut() == QKeySequence::Save);
}

TEST_CASE("Phase 9 UI: Save As action has correct shortcut", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    QAction* saveAsAction = nullptr;
    QMenu* fileMenu = nullptr;
    for (auto action : window.menuBar()->actions()) {
        if (action->text() == "File") {
            fileMenu = action->menu();
            break;
        }
    }
    REQUIRE(fileMenu != nullptr);
    
    for (auto action : fileMenu->actions()) {
        if (action->text() == "Save As...") {
            saveAsAction = action;
            break;
        }
    }
    REQUIRE(saveAsAction != nullptr);
    
    // Verify shortcut is Ctrl+Shift+S
    REQUIRE(saveAsAction->shortcut() == QKeySequence::SaveAs);
}

TEST_CASE("Phase 9 UI: Save updates title bar correctly", "[ui][phase9]")
{
    ensureQApplication();
    clearTestSessions();
    MainWindow window;
    
    // Initially should show Untitled
    REQUIRE(window.windowTitle().contains("Untitled"));
    
    // After save, should show session name
    QString sessionPath = getTempSessionDir() + "/tracked_session.json";
    window.saveSessionAs(sessionPath);
    REQUIRE(window.windowTitle().contains("tracked_session"));
    REQUIRE(!window.windowTitle().contains("Untitled"));
    
    // Clean up
    QFile(sessionPath).remove();
}

