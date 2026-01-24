#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QStringList>
#include <QPushButton>

#include "../src/PreferencesDialog.h"

TEST_CASE("PreferencesDialog has 8 categories and basic buttons", "[preferences][dialog]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    PreferencesDialog dlg;

    // Verify category names
    QStringList names = dlg.categoryNames();
    REQUIRE(names.size() == 8);
    REQUIRE(names[0] == "Audio Engine");
    REQUIRE(names[1] == "Grid & Layout");
    REQUIRE(names[2] == "Waveform Cache");
    REQUIRE(names[3] == "File Handling");
    REQUIRE(names[4] == "Keyboard & Shortcuts");
    REQUIRE(names[5] == "Startup");
    REQUIRE(names[6] == "Debug");
    REQUIRE(names[7] == "Keep-Alive");

    // Verify buttons exist (by object name)
    auto btnCancel = dlg.findChild<QPushButton*>("btnCancel");
    auto btnSave = dlg.findChild<QPushButton*>("btnSave");
    auto btnReset = dlg.findChild<QPushButton*>("btnReset");
    REQUIRE(btnCancel != nullptr);
    REQUIRE(btnSave != nullptr);
    REQUIRE(btnReset != nullptr);
}
