#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QTemporaryDir>

#include "../src/SessionManager.h"
#include "../src/PreferencesManager.h"
#include "../src/MainWindow.h"

static QString testSessionDir()
{
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return tmpDir + "/libresoundboard_test_sessions";
}

static void clearSessions()
{
    QStandardPaths::setTestModeEnabled(true);
    
    // Clear all settings
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
    s.sync();
    
    // Clear singleton state
    SessionManager::instance().clearRecentSessions();
    SessionManager::instance().setCurrentSessionPath(QString());
    
    // Clear directory
    QDir dir(testSessionDir());
    dir.removeRecursively();
    dir.mkpath(".");
}

TEST_CASE("SessionManager saves session to file", "[sessions][io][phase9]") {
    clearSessions();
    
    QString sessionPath = testSessionDir() + "/test_session.json";
    QJsonObject root;
    QJsonArray tabs;
    QJsonArray titles;
    
    // Create minimal session structure
    root["titles"] = titles;
    root["tabs"] = tabs;
    
    QJsonDocument doc(root);
    
    // SessionManager should persist this
    REQUIRE(SessionManager::instance().saveSession(sessionPath, doc));
    REQUIRE(QFile::exists(sessionPath));
    
    QFile f(sessionPath);
    REQUIRE(f.open(QIODevice::ReadOnly));
    QJsonDocument loaded = QJsonDocument::fromJson(f.readAll());
    f.close();
    
    REQUIRE(loaded.isObject());
    REQUIRE(loaded.object().contains("tabs"));
    REQUIRE(loaded.object().contains("titles"));
}

TEST_CASE("SessionManager loads session from file", "[sessions][io][phase9]") {
    clearSessions();
    
    QString sessionPath = testSessionDir() + "/load_test.json";
    
    // Create a session file
    QJsonObject root;
    QJsonArray titles;
    titles.append("Board 1");
    root["titles"] = titles;
    root["tabs"] = QJsonArray();
    
    QFile f(sessionPath);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(QJsonDocument(root).toJson());
    f.close();
    
    // Load it via SessionManager
    QJsonDocument loaded = SessionManager::instance().loadSession(sessionPath);
    REQUIRE(loaded.isObject());
    REQUIRE(loaded.object().value("titles").toArray().size() == 1);
}

TEST_CASE("SessionManager tracks current session path", "[sessions][state][phase9]") {
    clearSessions();
    // Force reload of singleton
    SessionManager::instance().clearRecentSessions();
    
    QString sessionPath = testSessionDir() + "/current.json";
    
    REQUIRE(SessionManager::instance().currentSessionPath().isEmpty());
    
    SessionManager::instance().setCurrentSessionPath(sessionPath);
    REQUIRE(SessionManager::instance().currentSessionPath() == sessionPath);
}

TEST_CASE("SessionManager manages recent sessions list", "[sessions][recent][phase9]") {
    clearSessions();
    SessionManager::instance().clearRecentSessions();
    
    QString session1 = testSessionDir() + "/session1.json";
    QString session2 = testSessionDir() + "/session2.json";
    
    SessionManager::instance().updateRecentSessions(session1);
    SessionManager::instance().updateRecentSessions(session2);
    
    QStringList recent = SessionManager::instance().recentSessions();
    REQUIRE(recent.size() == 2);
    REQUIRE(recent[0] == session2);  // Most recent first
    REQUIRE(recent[1] == session1);
}

TEST_CASE("SessionManager persists recent sessions across instances", "[sessions][recent][persistence][phase9]") {
    clearSessions();
    
    QString session1 = testSessionDir() + "/persist1.json";
    QString session2 = testSessionDir() + "/persist2.json";
    
    // Create actual session files
    QFile f1(session1);
    REQUIRE(f1.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f1.write(QJsonDocument(QJsonObject()).toJson());
    f1.close();
    
    QFile f2(session2);
    REQUIRE(f2.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f2.write(QJsonDocument(QJsonObject()).toJson());
    f2.close();
    
    SessionManager::instance().updateRecentSessions(session1);
    SessionManager::instance().updateRecentSessions(session2);
    SessionManager::instance().saveRecentSessions();
    
    // Create new list and reload to simulate new instance
    QStringList temp;
    SessionManager::instance().recentSessions();  // Current state
    SessionManager::instance().loadRecentSessions();  // Reload from saved
    
    QStringList recent = SessionManager::instance().recentSessions();
    REQUIRE(recent.size() >= 1);
    REQUIRE(recent.contains(session1));
    REQUIRE(recent.contains(session2));
}

TEST_CASE("SessionManager removes non-existent files from recent", "[sessions][recent][cleanup][phase9]") {
    clearSessions();
    SessionManager::instance().clearRecentSessions();
    
    QString validSession = testSessionDir() + "/valid.json";
    QString missingSession = testSessionDir() + "/missing.json";
    
    // Create valid file
    QFile fvalid(validSession);
    REQUIRE(fvalid.open(QIODevice::WriteOnly | QIODevice::Truncate));
    fvalid.write(QJsonDocument(QJsonObject()).toJson());
    fvalid.close();
    
    // Don't create missing file (it will be non-existent)
    
    SessionManager::instance().updateRecentSessions(validSession);
    SessionManager::instance().updateRecentSessions(missingSession);
    SessionManager::instance().saveRecentSessions();
    
    SessionManager::instance().cleanRecentSessions();
    
    QStringList recent = SessionManager::instance().recentSessions();
    REQUIRE(recent.size() == 1);
    REQUIRE(recent[0] == validSession);
}

TEST_CASE("MainWindow saves/loads session with current path tracking", "[sessions][mainwindow][phase9]") {
    clearSessions();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);
    
    QString sessionPath = testSessionDir() + "/mw_session.json";
    
    {
        MainWindow mw;
        mw.saveSessionAs(sessionPath);
        
        REQUIRE(SessionManager::instance().currentSessionPath() == sessionPath);
    }
    
    // Load and verify
    MainWindow mw2;
    mw2.loadSession(sessionPath);
    
    REQUIRE(SessionManager::instance().currentSessionPath() == sessionPath);
}

TEST_CASE("MainWindow respects startup preference with session path", "[sessions][startup][phase9]") {
    clearSessions();
    
    QString sessionFile = testSessionDir() + "/startup_session.json";
    
    // Create a session file with known content
    QJsonObject root;
    QJsonArray tabs;
    QJsonArray tab0;
    QJsonObject slot0;
    slot0["file"] = "startup_test.wav";
    slot0["volume"] = 0.6;
    tab0.append(slot0);
    tabs.append(tab0);
    
    QJsonArray titles;
    titles.append("Startup Board");
    root["titles"] = titles;
    root["tabs"] = tabs;
    
    QFile f(sessionFile);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(QJsonDocument(root).toJson());
    f.close();
    
    // Set preference to restore and point to this session
    PreferencesManager::instance().setStartupBehavior(PreferencesManager::StartupBehavior::RestoreLastSession);
    PreferencesManager::instance().setLastSavedSessionPath(sessionFile);
    
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);
    
    MainWindow mw;
    REQUIRE(SessionManager::instance().currentSessionPath() == sessionFile);
}
