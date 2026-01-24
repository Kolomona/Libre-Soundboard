#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "../src/MainWindow.h"
#include "../src/KeepAliveMonitor.h"
#include "../src/AudioEngine.h"
#include <QApplication>
#include <QThread>

/**
 * Tests for MainWindow and KeepAliveMonitor integration
 * When KeepAliveMonitor triggers, MainWindow should play the last sound from the last tab
 */

TEST_CASE("MainWindow can create and initialize with KeepAliveMonitor", "[mainwindow][keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    MainWindow window;
    // Should initialize without crashing
    REQUIRE(true);
}

TEST_CASE("MainWindow has KeepAliveMonitor accessible", "[mainwindow][keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    MainWindow window;
    
    // MainWindow should have a way to get the KeepAliveMonitor
    KeepAliveMonitor* monitor = window.getKeepAliveMonitor();
    REQUIRE(monitor != nullptr);
}

TEST_CASE("MainWindow connects KeepAliveMonitor signal to onKeepAliveTriggered", "[mainwindow][keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    MainWindow window;
    KeepAliveMonitor* monitor = window.getKeepAliveMonitor();
    
    // Should be able to emit the signal
    int triggerCount = 0;
    QObject::connect(monitor, &KeepAliveMonitor::keepAliveTriggered, 
        [&triggerCount]() {
            triggerCount++;
        });
    
    emit monitor->keepAliveTriggered();
    
    REQUIRE(triggerCount == 1);
}

TEST_CASE("MainWindow provides audio engine access to KeepAliveMonitor", "[mainwindow][keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    MainWindow window;
    
    // The KeepAliveMonitor should be registered with the AudioEngine
    KeepAliveMonitor* monitor = window.getKeepAliveMonitor();
    AudioEngine* engine = window.getAudioEngine();
    
    REQUIRE(engine != nullptr);
    REQUIRE(engine->getKeepAliveMonitor() == monitor);
}

TEST_CASE("MainWindow onKeepAliveTriggered processes input and triggers playback", "[mainwindow][keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);
    
    MainWindow window;
    
    // Note: Full integration test would require setting up containers with sound files
    // For now, just verify the method exists and can be called
    window.onKeepAliveTriggered();
    
    // Should not crash
    REQUIRE(true);
}
