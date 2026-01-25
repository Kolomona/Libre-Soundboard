#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QTreeWidget>
#include <QRadioButton>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "../src/PreferencesManager.h"
#include "../src/PreferencesDialog.h"
#include "../src/MainWindow.h"
#include "../src/SoundContainer.h"

static QString layoutPath()
{
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/libresoundboard";
    QDir d(cfgDir);
    if (!d.exists()) {
        d.mkpath(".");
    }
    return cfgDir + "/layout.json";
}

static void clearPrefs()
{
    QStandardPaths::setTestModeEnabled(true);
    PreferencesManager::instance().settings().clear();
    PreferencesManager::instance().settings().sync();
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
    s.sync();
    QFile::remove(layoutPath());
}

static void writeLayoutWithFile(const QString& file)
{
    QJsonArray tabs;
    // first tab with one slot populated
    QJsonArray tab0;
    QJsonObject slot0;
    slot0["file"] = file;
    slot0["volume"] = 0.5;
    tab0.append(slot0);
    tabs.append(tab0);
    // remaining tabs empty
    for (int i = 1; i < 4; ++i) {
        tabs.append(QJsonArray());
    }

    QJsonArray titles;
    titles.append(QStringLiteral("Saved Board 1"));
    titles.append(QStringLiteral("Saved Board 2"));
    titles.append(QStringLiteral("Saved Board 3"));
    titles.append(QStringLiteral("Saved Board 4"));

    QJsonObject root;
    root["titles"] = titles;
    root["tabs"] = tabs;

    QFile f(layoutPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root);
        f.write(doc.toJson());
        f.close();
    }
}

TEST_CASE("Startup preference default and persistence", "[preferences][startup][phase8]") {
    clearPrefs();
    PreferencesManager& pm = PreferencesManager::instance();

    REQUIRE(pm.startupBehavior() == PreferencesManager::StartupBehavior::RestoreLastSession);

    pm.setStartupBehavior(PreferencesManager::StartupBehavior::StartEmpty);
    QSettings s("libresoundboard", "libresoundboard");
    REQUIRE(s.value("startup/behavior").toInt() == 1);
    REQUIRE(pm.startupBehavior() == PreferencesManager::StartupBehavior::StartEmpty);
}

TEST_CASE("Startup preferences page UI", "[preferences][startup][dialog][phase8]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->setCurrentItem(tree->topLevelItem(5));

    auto radioRestore = dlg.findChild<QRadioButton*>("radioStartupRestore");
    auto radioEmpty = dlg.findChild<QRadioButton*>("radioStartupEmpty");
    REQUIRE(radioRestore != nullptr);
    REQUIRE(radioEmpty != nullptr);

    REQUIRE(radioRestore->isChecked());
    REQUIRE_FALSE(radioEmpty->isChecked());

    radioEmpty->setChecked(true);

    auto btnSave = dlg.findChild<QPushButton*>("btnSave");
    REQUIRE(btnSave != nullptr);
    btnSave->click();

    REQUIRE(PreferencesManager::instance().startupBehavior() == PreferencesManager::StartupBehavior::StartEmpty);

    PreferencesDialog dlg2;
    auto tree2 = dlg2.findChild<QTreeWidget*>();
    REQUIRE(tree2 != nullptr);
    tree2->setCurrentItem(tree2->topLevelItem(5));

    auto radioRestore2 = dlg2.findChild<QRadioButton*>("radioStartupRestore");
    auto radioEmpty2 = dlg2.findChild<QRadioButton*>("radioStartupEmpty");
    REQUIRE(radioRestore2 != nullptr);
    REQUIRE(radioEmpty2 != nullptr);

    REQUIRE_FALSE(radioRestore2->isChecked());
    REQUIRE(radioEmpty2->isChecked());
}

TEST_CASE("MainWindow honors startup behavior", "[preferences][startup][mainwindow][phase8]") {
    clearPrefs();
    writeLayoutWithFile(QStringLiteral("restore.wav"));
    PreferencesManager::instance().setStartupBehavior(PreferencesManager::StartupBehavior::RestoreLastSession);

    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    {
        MainWindow mw;
        SoundContainer* sc = mw.containerAt(0, 0);
        REQUIRE(sc != nullptr);
        REQUIRE(sc->file() == QStringLiteral("restore.wav"));
    }

    // Now set start-empty and ensure saved layout is ignored on startup
    PreferencesManager::instance().setStartupBehavior(PreferencesManager::StartupBehavior::StartEmpty);
    writeLayoutWithFile(QStringLiteral("should-not-load.wav"));

    MainWindow mw2;
    SoundContainer* sc2 = mw2.containerAt(0, 0);
    REQUIRE(sc2 != nullptr);
    REQUIRE(sc2->file().isEmpty());
}
