#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QTreeWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <fstream>
#include <sys/stat.h>

#include "../src/PreferencesManager.h"
#include "../src/PreferencesDialog.h"
#include "../src/MainWindow.h"
#include "../src/AudioEngine.h"

static void clearPrefs()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
}

TEST_CASE("JACK prefs default and persistence", "[preferences][jack][phase6]") {
    clearPrefs();
    PreferencesManager& pm = PreferencesManager::instance();

    REQUIRE(pm.jackClientName() == QStringLiteral("libre-soundboard"));
    REQUIRE(pm.jackRememberConnections() == true);

    pm.setJackClientName("vibe-test");
    pm.setJackRememberConnections(false);

    QSettings s("libresoundboard", "libresoundboard");
    REQUIRE(s.value("audio/jackClientName").toString() == QStringLiteral("vibe-test"));
    REQUIRE(s.value("audio/jackRememberConnections").toBool() == false);
}

TEST_CASE("Audio page edits JACK preferences", "[preferences][dialog][phase6]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->setCurrentItem(tree->topLevelItem(0));

    auto nameEdit = dlg.findChild<QLineEdit*>("editJackClientName");
    auto chkRemember = dlg.findChild<QCheckBox*>("chkJackRememberConnections");
    REQUIRE(nameEdit != nullptr);
    REQUIRE(chkRemember != nullptr);

    REQUIRE(nameEdit->text() == QStringLiteral("libre-soundboard"));
    REQUIRE(chkRemember->isChecked() == true);

    nameEdit->setText("phase6-client");
    chkRemember->setChecked(false);

    auto btnSave = dlg.findChild<QPushButton*>("btnSave");
    REQUIRE(btnSave != nullptr);
    btnSave->click();

    PreferencesManager& pm = PreferencesManager::instance();
    REQUIRE(pm.jackClientName() == QStringLiteral("phase6-client"));
    REQUIRE(pm.jackRememberConnections() == false);

    PreferencesDialog dlg2;
    auto tree2 = dlg2.findChild<QTreeWidget*>();
    REQUIRE(tree2 != nullptr);
    tree2->setCurrentItem(tree2->topLevelItem(0));

    auto nameEdit2 = dlg2.findChild<QLineEdit*>("editJackClientName");
    auto chkRemember2 = dlg2.findChild<QCheckBox*>("chkJackRememberConnections");
    REQUIRE(nameEdit2 != nullptr);
    REQUIRE(chkRemember2 != nullptr);

    REQUIRE(nameEdit2->text() == QStringLiteral("phase6-client"));
    REQUIRE(chkRemember2->isChecked() == false);
}

TEST_CASE("MainWindow restarts audio engine when JACK prefs change", "[preferences][jack][mainwindow][phase6]") {
    clearPrefs();
    QStandardPaths::setTestModeEnabled(true);
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    MainWindow mw;
    AudioEngine* engine = mw.getAudioEngine();
    REQUIRE(engine != nullptr);
    int initialInits = engine->initCount();
    std::string initialName = engine->clientName();

    PreferencesDialog dlg(&mw);
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->setCurrentItem(tree->topLevelItem(0));

    auto nameEdit = dlg.findChild<QLineEdit*>("editJackClientName");
    auto chkRemember = dlg.findChild<QCheckBox*>("chkJackRememberConnections");
    REQUIRE(nameEdit != nullptr);
    REQUIRE(chkRemember != nullptr);

    nameEdit->setText(QStringLiteral("mw-phase6"));
    chkRemember->setChecked(!chkRemember->isChecked());

    auto btnSave = dlg.findChild<QPushButton*>("btnSave");
    REQUIRE(btnSave != nullptr);
    btnSave->click();

    REQUIRE(engine->initCount() == initialInits + 1);
    REQUIRE(engine->clientName() == std::string("mw-phase6"));
    REQUIRE(engine->autoConnectOutputsEnabled() == false);
    REQUIRE(engine->clientName() != initialName);
}

TEST_CASE("Client name change preserves connections", "[preferences][jack][connections][phase6]") {
    clearPrefs();
    
    // Create a mock connections file with old client name
    std::string configPath = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + 
                             "/.config/libresoundboard/jack_connections.cfg";
    
    // Create directory if needed
    std::string dir = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/.config/libresoundboard";
    mkdir(dir.c_str(), 0700);
    
    // Write connections with old client name
    std::ofstream ofs(configPath, std::ios::trunc);
    REQUIRE(ofs.is_open());
    ofs << "old-client:out_l|system:playback_1\n";
    ofs << "old-client:out_r|system:playback_2\n";
    ofs << "system:capture_1|old-client:in\n";
    ofs.close();
    
    // Call the update function
    AudioEngine::updateConnectionsForClientRename("old-client", "new-client");
    
    // Read back and verify names were updated
    std::ifstream ifs(configPath);
    REQUIRE(ifs.is_open());
    
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ifs, line)) {
        lines.push_back(line);
    }
    ifs.close();
    
    REQUIRE(lines.size() == 3);
    REQUIRE(lines[0] == "new-client:out_l|system:playback_1");
    REQUIRE(lines[1] == "new-client:out_r|system:playback_2");
    REQUIRE(lines[2] == "system:capture_1|new-client:in");
}
