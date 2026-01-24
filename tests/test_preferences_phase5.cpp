#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QSpinBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QTemporaryFile>
#include <QColor>

#include "../src/PreferencesManager.h"
#include "../src/PreferencesDialog.h"
#include "../src/PreferencesPages.h"
#include "../src/MainWindow.h"
#include "../src/SoundContainer.h"

static void clearPrefs()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
}

TEST_CASE("Grid prefs default and persistence", "[preferences][grid][phase5]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();
    REQUIRE(pm.gridRows() == 4);
    REQUIRE(pm.gridCols() == 8);

    pm.setGridRows(7);
    pm.setGridCols(12);
    REQUIRE(pm.gridRows() == 7);
    REQUIRE(pm.gridCols() == 12);

    QSettings s("libresoundboard", "libresoundboard");
    REQUIRE(s.value("grid/rows").toInt() == 7);
    REQUIRE(s.value("grid/cols").toInt() == 12);
}

TEST_CASE("Grid prefs clamp to limits", "[preferences][grid][phase5]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();
    pm.setGridRows(1);
    pm.setGridCols(30);
    REQUIRE(pm.gridRows() == 2);
    REQUIRE(pm.gridCols() == 16);
}

TEST_CASE("Grid layout page applies and emits change", "[preferences][grid][dialog][phase5]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->setCurrentItem(tree->topLevelItem(1));

    auto spinRows = dlg.findChild<QSpinBox*>("spinGridRows");
    auto spinCols = dlg.findChild<QSpinBox*>("spinGridCols");
    REQUIRE(spinRows != nullptr);
    REQUIRE(spinCols != nullptr);

    // Default values shown
    REQUIRE(spinRows->value() == 4);
    REQUIRE(spinCols->value() == 8);

    bool emitted = false;
    auto page = dlg.findChild<PrefGridLayoutPage*>();
    REQUIRE(page != nullptr);
    QObject::connect(page, &PrefGridLayoutPage::dimensionsChanged, [&emitted](int r, int c){
        emitted = true;
        REQUIRE(r == 6);
        REQUIRE(c == 10);
    });

    spinRows->setValue(6);
    spinCols->setValue(10);

    // Click Save
    auto saveBtn = dlg.findChild<QPushButton*>("btnSave");
    REQUIRE(saveBtn != nullptr);
    saveBtn->click();

    REQUIRE(PreferencesManager::instance().gridRows() == 6);
    REQUIRE(PreferencesManager::instance().gridCols() == 10);
    REQUIRE(emitted == true);
}

TEST_CASE("MainWindow rebuild preserves grid assignments", "[preferences][grid][mainwindow][phase5]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager::instance().setGridRows(4);
    PreferencesManager::instance().setGridCols(8);

    MainWindow w;

    // Prepare a temp file to use as a sound placeholder
    QTemporaryFile tmpFile;
    REQUIRE(tmpFile.open());
    QString path = tmpFile.fileName();

    auto c0 = w.containerAt(0, 0);
    REQUIRE(c0 != nullptr);
    c0->setFile(path);
    c0->setVolume(0.25f);
    c0->setBackdropColor(QColor(Qt::red));

    auto c1 = w.containerAt(0, 1);
    REQUIRE(c1 != nullptr);
    c1->setFile(path);
    c1->setVolume(0.5f);

    w.onGridDimensionsChanged(2, 4);

    REQUIRE(w.gridRows() == 2);
    REQUIRE(w.gridCols() == 4);

    auto n0 = w.containerAt(0, 0);
    auto n1 = w.containerAt(0, 1);
    REQUIRE(n0 != nullptr);
    REQUIRE(n1 != nullptr);
    REQUIRE(n0->file() == path);
    REQUIRE(n1->file() == path);
    REQUIRE(n0->volume() == Approx(0.25f));
    REQUIRE(n1->volume() == Approx(0.5f));
    REQUIRE(n0->backdropColor() == QColor(Qt::red));

    // New grid size should match rows*cols
    REQUIRE(w.containerCountForTab(0) == 8);
}
