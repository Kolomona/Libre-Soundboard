#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QStandardPaths>
#include <QSettings>
#include <QTreeWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QElapsedTimer>
#include <QThread>

#include "../src/PreferencesManager.h"
#include "../src/PreferencesDialog.h"
#include "../src/MainWindow.h"
#include "../src/KeepAliveMonitor.h"

static void clearPrefs()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings s("libresoundboard", "libresoundboard");
    s.clear();
}

TEST_CASE("KeepAlive preferences default and persistence", "[preferences][keepalive][phase3]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesManager& pm = PreferencesManager::instance();

    REQUIRE(pm.keepAliveEnabled() == true);
    REQUIRE(pm.keepAliveTimeoutSeconds() == 60);
    REQUIRE(pm.keepAliveSensitivityDbfs() == Approx(-60.0));
    REQUIRE(pm.keepAliveAnyNonZero() == false);
    REQUIRE(pm.keepAliveTarget() == PreferencesManager::KeepAliveTarget::LastTabLastSound);
    REQUIRE(pm.keepAliveUseSlotVolume() == true);
    REQUIRE(pm.keepAliveOverrideVolume() == Approx(1.0));
    REQUIRE(pm.keepAliveAutoConnectInput() == true);

    pm.setKeepAliveEnabled(false);
    pm.setKeepAliveTimeoutSeconds(15);
    pm.setKeepAliveSensitivityDbfs(-50.0);
    pm.setKeepAliveAnyNonZero(true);
    pm.setKeepAliveTarget(PreferencesManager::KeepAliveTarget::SpecificSlot);
    pm.setKeepAliveTargetTab(2);
    pm.setKeepAliveTargetSlot(5);
    pm.setKeepAliveUseSlotVolume(false);
    pm.setKeepAliveOverrideVolume(0.25);
    pm.setKeepAliveAutoConnectInput(false);

    REQUIRE(pm.keepAliveEnabled() == false);
    REQUIRE(pm.keepAliveTimeoutSeconds() == 15);
    REQUIRE(pm.keepAliveSensitivityDbfs() == Approx(-50.0));
    REQUIRE(pm.keepAliveAnyNonZero() == true);
    REQUIRE(pm.keepAliveTarget() == PreferencesManager::KeepAliveTarget::SpecificSlot);
    REQUIRE(pm.keepAliveTargetTab() == 2);
    REQUIRE(pm.keepAliveTargetSlot() == 5);
    REQUIRE(pm.keepAliveUseSlotVolume() == false);
    REQUIRE(pm.keepAliveOverrideVolume() == Approx(0.25));
    REQUIRE(pm.keepAliveAutoConnectInput() == false);
}

TEST_CASE("KeepAlive preferences dialog saves and restores", "[preferences][keepalive][dialog][phase3]") {
    clearPrefs();
    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);

    PreferencesDialog dlg;
    auto tree = dlg.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->setCurrentItem(tree->topLevelItem(7));

    auto chkEnable = dlg.findChild<QCheckBox*>("chkKeepAliveEnable");
    auto spinTimeout = dlg.findChild<QSpinBox*>("spinKeepAliveTimeout");
    auto spinSens = dlg.findChild<QDoubleSpinBox*>("spinKeepAliveSensitivity");
    auto chkAny = dlg.findChild<QCheckBox*>("chkKeepAliveAnyNonZero");
    auto comboTarget = dlg.findChild<QComboBox*>("comboKeepAliveTarget");
    auto spinTab = dlg.findChild<QSpinBox*>("spinKeepAliveTab");
    auto spinSlot = dlg.findChild<QSpinBox*>("spinKeepAliveSlot");
    auto chkUseSlotVol = dlg.findChild<QCheckBox*>("chkKeepAliveUseSlotVolume");
    auto spinOverride = dlg.findChild<QDoubleSpinBox*>("spinKeepAliveOverrideVolume");
    auto chkAutoConnect = dlg.findChild<QCheckBox*>("chkKeepAliveAutoConnect");
    REQUIRE(chkEnable != nullptr);
    REQUIRE(spinTimeout != nullptr);
    REQUIRE(spinSens != nullptr);
    REQUIRE(chkAny != nullptr);
    REQUIRE(comboTarget != nullptr);
    REQUIRE(spinTab != nullptr);
    REQUIRE(spinSlot != nullptr);
    REQUIRE(chkUseSlotVol != nullptr);
    REQUIRE(spinOverride != nullptr);
    REQUIRE(chkAutoConnect != nullptr);

    // defaults shown
    REQUIRE(chkEnable->isChecked() == true);
    REQUIRE(spinTimeout->value() == 60);
    REQUIRE(spinSens->value() == Approx(-60.0));
    REQUIRE(chkAny->isChecked() == false);
    REQUIRE(comboTarget->currentIndex() == 0);
    REQUIRE(chkUseSlotVol->isChecked() == true);
    REQUIRE(chkAutoConnect->isChecked() == true);

    // Modify and save
    chkEnable->setChecked(false);
    spinTimeout->setValue(10);
    spinSens->setValue(-48.0);
    chkAny->setChecked(true);
    comboTarget->setCurrentIndex(1);
    spinTab->setValue(2);
    spinSlot->setValue(3);
    chkUseSlotVol->setChecked(false);
    spinOverride->setValue(0.42);
    chkAutoConnect->setChecked(false);

    auto btnSave = dlg.findChild<QPushButton*>("btnSave");
    REQUIRE(btnSave != nullptr);
    btnSave->click();

    PreferencesManager& pm = PreferencesManager::instance();
    REQUIRE(pm.keepAliveEnabled() == false);
    REQUIRE(pm.keepAliveTimeoutSeconds() == 10);
    REQUIRE(pm.keepAliveAnyNonZero() == true);
    REQUIRE(pm.keepAliveTarget() == PreferencesManager::KeepAliveTarget::SpecificSlot);
    REQUIRE(pm.keepAliveTargetTab() == 1); // zero-based stored
    REQUIRE(pm.keepAliveTargetSlot() == 2);
    REQUIRE(pm.keepAliveUseSlotVolume() == false);
    REQUIRE(pm.keepAliveOverrideVolume() == Approx(0.42));
    REQUIRE(pm.keepAliveAutoConnectInput() == false);

    PreferencesDialog dlg2;
    auto tree2 = dlg2.findChild<QTreeWidget*>();
    tree2->setCurrentItem(tree2->topLevelItem(7));
    auto chkEnable2 = dlg2.findChild<QCheckBox*>("chkKeepAliveEnable");
    auto spinTimeout2 = dlg2.findChild<QSpinBox*>("spinKeepAliveTimeout");
    auto chkAny2 = dlg2.findChild<QCheckBox*>("chkKeepAliveAnyNonZero");
    auto comboTarget2 = dlg2.findChild<QComboBox*>("comboKeepAliveTarget");
    auto spinTab2 = dlg2.findChild<QSpinBox*>("spinKeepAliveTab");
    auto spinSlot2 = dlg2.findChild<QSpinBox*>("spinKeepAliveSlot");
    auto chkUseSlotVol2 = dlg2.findChild<QCheckBox*>("chkKeepAliveUseSlotVolume");
    auto spinOverride2 = dlg2.findChild<QDoubleSpinBox*>("spinKeepAliveOverrideVolume");
    auto chkAutoConnect2 = dlg2.findChild<QCheckBox*>("chkKeepAliveAutoConnect");

    REQUIRE(chkEnable2->isChecked() == false);
    REQUIRE(spinTimeout2->value() == 10);
    REQUIRE(chkAny2->isChecked() == true);
    REQUIRE(comboTarget2->currentIndex() == 1);
    REQUIRE(spinTab2->value() == 2);
    REQUIRE(spinSlot2->value() == 3);
    REQUIRE(chkUseSlotVol2->isChecked() == false);
    REQUIRE(spinOverride2->value() == Approx(0.42));
    REQUIRE(chkAutoConnect2->isChecked() == false);
}

TEST_CASE("KeepAlive preferences apply on startup", "[preferences][keepalive][startup][phase3]") {
    clearPrefs();
    QStandardPaths::setTestModeEnabled(true);
    PreferencesManager& pm = PreferencesManager::instance();
    pm.setKeepAliveEnabled(true);
    pm.setKeepAliveTimeoutSeconds(1);
    pm.setKeepAliveSensitivityDbfs(-40.0);
    pm.setKeepAliveAnyNonZero(false);

    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);
    MainWindow mw;

    auto label = mw.findChild<QLabel*>("keepAliveStatusLabel");
    REQUIRE(label != nullptr);
    REQUIRE(label->isHidden() == false);
    REQUIRE(label->text().contains("KeepAlive: Active"));

    KeepAliveMonitor* mon = mw.getKeepAliveMonitor();
    REQUIRE(mon != nullptr);
    REQUIRE(mon->silenceTimeoutMs() == 1000);
    REQUIRE(mon->sensitivityDbfs() == Approx(-40.0));
}

TEST_CASE("KeepAlive disabled prevents trigger", "[preferences][keepalive][status][phase3]") {
    clearPrefs();
    QStandardPaths::setTestModeEnabled(true);
    PreferencesManager& pm = PreferencesManager::instance();
    pm.setKeepAliveEnabled(false);
    pm.setKeepAliveTimeoutSeconds(1);

    int argc = 0; char* argv[] = {nullptr}; QApplication app(argc, argv);
    MainWindow mw;

    auto label = mw.findChild<QLabel*>("keepAliveStatusLabel");
    REQUIRE(label != nullptr);
    REQUIRE(label->isHidden() == true);

    KeepAliveMonitor* mon = mw.getKeepAliveMonitor();
    REQUIRE(mon != nullptr);
    bool triggered = false;
    QObject::connect(mon, &KeepAliveMonitor::keepAliveTriggered, [&](){ triggered = true; });

    std::vector<float> silent = {0.0f};
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < 1500) {
        mon->processInputSamples(silent, 1, 1);
        QThread::msleep(50);
    }

    REQUIRE(triggered == false);
}
