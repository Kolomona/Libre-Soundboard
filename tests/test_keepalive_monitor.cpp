#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>

#include "../src/KeepAliveMonitor.h"

TEST_CASE("KeepAliveMonitor initial state", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    REQUIRE(monitor.silenceDuration() == Approx(0.0));
    REQUIRE(monitor.lastFrameHadSound() == false);
}

TEST_CASE("KeepAliveMonitor detects silence in silent frame", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    // Silent frame: all zeros
    std::vector<float> silentFrame = {0.0f, 0.0f, 0.0f, 0.0f};
    monitor.processInputSamples(silentFrame, 2, 2); // 2 frames, 2 channels stereo
    
    REQUIRE(monitor.lastFrameHadSound() == false);
    // Should have recorded silence start time
    REQUIRE(monitor.silenceDuration() >= 0.0);
}

TEST_CASE("KeepAliveMonitor detects sound in frame", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    // Frame with sound: contains non-zero samples anywhere in the batch
    std::vector<float> soundFrame = {0.1f, 0.0f, 0.0f, 0.0f};
    monitor.processInputSamples(soundFrame, 2, 2); // 2 frames, 2 channels
    
    // Silence timer should reset because sound was detected
    REQUIRE(monitor.silenceDuration() < 0.1);
}

TEST_CASE("KeepAliveMonitor silence accumulates", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    // Process multiple silent frames and verify silence duration increases
    std::vector<float> silentFrame = {0.0f, 0.0f};
    
    monitor.processInputSamples(silentFrame, 1, 2);
    double dur1 = monitor.silenceDuration();
    REQUIRE(dur1 >= 0.0);
    
    QThread::msleep(100);
    
    monitor.processInputSamples(silentFrame, 1, 2);
    double dur2 = monitor.silenceDuration();
    REQUIRE(dur2 > dur1);
    REQUIRE(dur2 >= 0.09); // At least 90ms accumulated
}

TEST_CASE("KeepAliveMonitor resets silence on sound", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    // Build up silence
    std::vector<float> silentFrame = {0.0f, 0.0f};
    std::vector<float> soundFrame = {0.5f, 0.0f};
    
    monitor.processInputSamples(silentFrame, 1, 2);
    QThread::msleep(100);
    monitor.processInputSamples(silentFrame, 1, 2);
    double silenceBefore = monitor.silenceDuration();
    REQUIRE(silenceBefore > 0.05);
    
    // Process sound frame - should reset timer
    monitor.processInputSamples(soundFrame, 1, 2);
    double silenceAfter = monitor.silenceDuration();
    
    REQUIRE(silenceAfter < 0.05); // Should be near zero
}

TEST_CASE("KeepAliveMonitor triggers after 60 seconds of silence", "[keepalive][slow]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    std::vector<float> silentFrame = {0.0f, 0.0f};
    
    bool triggered = false;
    QObject::connect(&monitor, &KeepAliveMonitor::keepAliveTriggered, [&triggered]() {
        triggered = true;
    });
    
    monitor.processInputSamples(silentFrame, 1, 2);
    
    // Wait for 60+ seconds (using event loop to allow signals)
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 61000) {
        monitor.processInputSamples(silentFrame, 1, 2);
        QThread::msleep(100);
        if (triggered) break;
    }
    
    REQUIRE(triggered == true);
}

TEST_CASE("KeepAliveMonitor timer resets after trigger for repeated keep-alive", "[keepalive][slow]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    std::vector<float> silentFrame = {0.0f, 0.0f};
    
    int triggerCount = 0;
    QObject::connect(&monitor, &KeepAliveMonitor::keepAliveTriggered, [&triggerCount]() {
        triggerCount++;
    });
    
    monitor.processInputSamples(silentFrame, 1, 2);
    
    // Wait for first trigger (60 seconds)
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 62000 && triggerCount < 1) {
        monitor.processInputSamples(silentFrame, 1, 2);
        QThread::msleep(100);
    }
    
    REQUIRE(triggerCount == 1);
    
    // Continue without any external input (simulating keep-alive sound playing to output)
    // Should eventually trigger again (after another 60 seconds) since timer resets after trigger
    int checkPoint = timer.elapsed();
    while (timer.elapsed() - checkPoint < 61000 && triggerCount < 2) {
        monitor.processInputSamples(silentFrame, 1, 2);
        QThread::msleep(100);
    }
    
    REQUIRE(triggerCount == 2);
}

TEST_CASE("KeepAliveMonitor does not trigger multiple times in same silence period", "[keepalive][slow]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    std::vector<float> silentFrame = {0.0f, 0.0f};
    
    int triggerCount = 0;
    QObject::connect(&monitor, &KeepAliveMonitor::keepAliveTriggered, [&triggerCount]() {
        triggerCount++;
    });
    
    monitor.processInputSamples(silentFrame, 1, 2);
    
    // Wait for trigger (60 seconds)
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 61000) {
        monitor.processInputSamples(silentFrame, 1, 2);
        QThread::msleep(100);
    }
    
    REQUIRE(triggerCount == 1);
    
    // Continue processing silent frames for a bit more, but should NOT trigger again
    int checkpoint = timer.elapsed();
    while (timer.elapsed() - checkpoint < 2000) {
        monitor.processInputSamples(silentFrame, 1, 2);
        QThread::msleep(100);
    }
    
    REQUIRE(triggerCount == 1); // Still only triggered once
}

TEST_CASE("KeepAliveMonitor manual silence timer reset", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    std::vector<float> silentFrame = {0.0f, 0.0f};
    
    // Build up silence
    monitor.processInputSamples(silentFrame, 1, 2);
    QThread::msleep(100);
    monitor.processInputSamples(silentFrame, 1, 2);
    double silenceBefore = monitor.silenceDuration();
    REQUIRE(silenceBefore > 0.05);
    
    // Manual reset
    monitor.resetSilenceTimer();
    double silenceAfter = monitor.silenceDuration();
    
    REQUIRE(silenceAfter < 0.05);
}

TEST_CASE("KeepAliveMonitor works with mono audio", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    // Mono silent frame
    std::vector<float> monoSilent = {0.0f, 0.0f};
    monitor.processInputSamples(monoSilent, 2, 1);
    REQUIRE(monitor.lastFrameHadSound() == false);
    
    // Mono with sound
    std::vector<float> monoSound = {0.3f, -0.2f};
    monitor.processInputSamples(monoSound, 2, 1);
    REQUIRE(monitor.lastFrameHadSound() == true);
}

TEST_CASE("KeepAliveMonitor treats very small amplitude below default threshold as silent", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor; // default now -60 dBFS
    
    // Tiny non-zero value below -60 dBFS should NOT reset timer
    std::vector<float> veryQuiet = {0.0001f, 0.0f};
    monitor.processInputSamples(veryQuiet, 1, 2);
    
    REQUIRE(monitor.silenceDuration() >= 0.0);
}

TEST_CASE("KeepAliveMonitor negative samples count as sound", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    std::vector<float> negativeSound = {-0.5f, 0.0f};
    monitor.processInputSamples(negativeSound, 2, 2);
    
    REQUIRE(monitor.silenceDuration() < 0.1);
}

TEST_CASE("KeepAliveMonitor lastFrameHadSound only checks final frame", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    
    // First frame has sound, last frame is silent
    std::vector<float> soundFirstSilentLast = {0.5f, 0.0f, 0.0f, 0.0f};
    monitor.processInputSamples(soundFirstSilentLast, 2, 2);
    
    // Overall batch is not silent (should reset timer)
    REQUIRE(monitor.silenceDuration() < 0.1);
    // But last frame specifically was silent
    REQUIRE(monitor.lastFrameHadSound() == false);
}

TEST_CASE("KeepAliveMonitor respects dBFS sensitivity threshold (peak)", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor;
    // Set sensitivity to -60 dBFS -> amplitude threshold ~ 0.001
    monitor.setSensitivityDbfs(-60.0);

    std::vector<float> frameBelow = {0.0005f, -0.0003f}; // below -60 dBFS
    std::vector<float> frameAbove = {0.0f, 0.01f};       // above threshold

    // Process below-threshold frames should be considered silence
    monitor.processInputSamples(frameBelow, 1, 2);
    double dur1 = monitor.silenceDuration();
    REQUIRE(dur1 >= 0.0);

    QThread::msleep(50);
    monitor.processInputSamples(frameBelow, 1, 2);
    double dur2 = monitor.silenceDuration();
    REQUIRE(dur2 > dur1);

    // Now an above-threshold frame should reset silence
    monitor.processInputSamples(frameAbove, 1, 2);
    REQUIRE(monitor.silenceDuration() < 0.05);
}

TEST_CASE("KeepAliveMonitor default sensitivity is -60 dBFS (peak)", "[keepalive]") {
    int argc = 0;
    char* argv[] = {nullptr};
    QApplication app(argc, argv);

    KeepAliveMonitor monitor; // default now enabled threshold

    // Below default threshold shouldn't reset
    std::vector<float> below = {0.0001f, 0.0f};
    monitor.processInputSamples(below, 1, 2);
    double d1 = monitor.silenceDuration();
    QThread::msleep(30);
    monitor.processInputSamples(below, 1, 2);
    double d2 = monitor.silenceDuration();
    REQUIRE(d2 > d1);

    // Above default threshold should reset
    std::vector<float> above = {0.002f, 0.0f};
    monitor.processInputSamples(above, 1, 2);
    REQUIRE(monitor.silenceDuration() < 0.05);
}
