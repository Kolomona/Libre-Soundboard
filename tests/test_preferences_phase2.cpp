#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>

#include "../src/PreferencesDialog.h"
#include "../src/PreferencesManager.h"
#include "../src/WaveformCache.h"
#include "../src/MainWindow.h"
#include "../src/SoundContainer.h"

static void clearPrefs()
{
    // Ensure tests use isolated config locations, never the user's real paths
    QStandardPaths::setTestModeEnabled(true);
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
}

TEST_CASE("Phase 2 UI widgets exist and persist via Save", "[preferences][phase2]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;

    // Navigate to Waveform Cache page
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    REQUIRE(tree->topLevelItemCount() >= 3);
    tree->setCurrentItem(tree->topLevelItem(2));

    // Expect spinboxes for cache size and TTL
    auto spinSize = dlg.findChild<QSpinBox*>("spinCacheSizeMB");
    auto spinTtl = dlg.findChild<QSpinBox*>("spinCacheTTLDays");
    REQUIRE(spinSize != nullptr);
    REQUIRE(spinTtl != nullptr);

    // Navigate to Debug page
    tree->setCurrentItem(tree->topLevelItem(6));
    auto comboLog = dlg.findChild<QComboBox*>("comboLogLevel");
    REQUIRE(comboLog != nullptr);

    // Set values and Save
    tree->setCurrentItem(tree->topLevelItem(2));
    spinSize->setValue(100);
    spinTtl->setValue(30);
    tree->setCurrentItem(tree->topLevelItem(6));
    comboLog->setCurrentIndex(3); // Info

    auto btnSave = dlg.findChild<QPushButton*>("btnSave");
    REQUIRE(btnSave != nullptr);
    btnSave->click();

    // Verify values persisted in PreferencesManager/QSettings
    PreferencesManager& pm = PreferencesManager::instance();
    REQUIRE(pm.settings().value("cache/softLimitMB").toInt() == 100);
    REQUIRE(pm.settings().value("cache/ttlDays").toInt() == 30);
    REQUIRE(pm.settings().value("debug/logLevel").toInt() == 3);

    // Reopen dialog and ensure widgets show persisted values
    PreferencesDialog dlg2;
    auto tree2 = dlg2.findChild<QTreeWidget*>();
    REQUIRE(tree2 != nullptr);
    tree2->setCurrentItem(tree2->topLevelItem(2));
    auto spinSize2 = dlg2.findChild<QSpinBox*>("spinCacheSizeMB");
    auto spinTtl2 = dlg2.findChild<QSpinBox*>("spinCacheTTLDays");
    REQUIRE(spinSize2->value() == 100);
    REQUIRE(spinTtl2->value() == 30);
    tree2->setCurrentItem(tree2->topLevelItem(6));
    auto comboLog2 = dlg2.findChild<QComboBox*>("comboLogLevel");
    REQUIRE(comboLog2->currentIndex() == 3);
}

TEST_CASE("MainWindow uses constant default gain for new containers", "[preferences][phase2]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    MainWindow mw;
    // Find a SoundContainer child and check initial volume
    auto containers = mw.findChildren<SoundContainer*>();
    REQUIRE(containers.size() > 0);
    REQUIRE(containers.first()->volume() == Approx(0.8f).margin(0.02f));
}

TEST_CASE("WaveformCache::evict honors preferences soft limit and TTL", "[preferences][phase2]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    // Set a small soft limit 1 MB and large TTL
    PreferencesManager::instance().settings().setValue("cache/softLimitMB", 1);
    PreferencesManager::instance().settings().setValue("cache/ttlDays", 3650);

    // Use a temporary cache directory
    QString tempDir = QDir::temp().filePath("libre_wave_cache_test");
    QDir d(tempDir);
    if (d.exists()) {
        for (const QString& f : d.entryList(QDir::Files)) QFile::remove(d.filePath(f));
    } else {
        d.mkpath(".");
    }
    qputenv("LIBRE_WAVEFORM_CACHE_DIR", tempDir.toUtf8());

    // Create 5 dummy pairs totaling about 1.5 MB
    QByteArray data(300000, '\x00'); // 300 KB
    for (int i = 0; i < 5; ++i) {
        QString base = QString("dummy_%1").arg(i);
        QFile png(d.filePath(base + ".png")); png.open(QIODevice::WriteOnly); png.write(data); png.close();
        QFile json(d.filePath(base + ".json")); json.open(QIODevice::WriteOnly); json.write("{}\n"); json.close();
    }

    // Evict and check total size <= 1 MB
    WaveformCache::evict();

    qint64 total = 0;
    for (const QString& f : d.entryList(QDir::Files)) {
        QFileInfo fi(d.filePath(f)); total += fi.size();
    }
    REQUIRE(total <= 1024 * 1024);
}
