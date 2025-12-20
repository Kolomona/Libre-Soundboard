#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QThread>
#include <QMetaObject>

#include "../src/PlayheadManager.h"
#include "../src/SoundContainer.h"
#include "../src/AudioEngine.h"

// Minimal test subclass to observe playhead callbacks
class TestContainer : public SoundContainer {
public:
    using SoundContainer::SoundContainer;
    void setPlayheadPosition(float pos) {
        m_lastPos = pos;
        m_history.push_back(pos);
    }
    float m_lastPos = -2.0f;
    std::vector<float> m_history;
};

TEST_CASE("PlayheadManager simulates playhead and respects unregister", "[playhead]") {
    int argc = 1;
    char* argv[] = {(char*)"test", nullptr};
    QApplication app(argc, argv);

    PlayheadManager* pm = PlayheadManager::instance();
    // Use a default AudioEngine instance (no JACK init). This will cause
    // getPlaybackInfoForId to report not-found, allowing the simulated
    // playback path to be used when playbackStarted() is called.
    AudioEngine engine;
    pm->init(&engine);

    TestContainer sc(nullptr);
    // register with a very short duration so the test runs quickly
    const QString id = "/tmp/fake_playhead.wav";

    // use a longer duration and short sleep so we catch an in-progress position
    pm->registerContainer(id, &sc, 1.0 /*sec*/, 48000);

    // start simulated playback
    pm->playbackStarted(id, &sc);

    // wait a short time (< duration) so simulation yields a fractional pos
    QThread::msleep(50);

    // trigger a tick synchronously
    bool invoked = QMetaObject::invokeMethod(pm, "onTick", Qt::DirectConnection);
    REQUIRE(invoked == true);

    // verify manager recorded a last-position for this container
    float last = pm->getLastPos(id, &sc);
    REQUIRE(last >= 0.0f);

    // unregister and ensure the manager no longer has an entry
    pm->unregisterContainer(id, &sc);
    QThread::msleep(30);
    invoked = QMetaObject::invokeMethod(pm, "onTick", Qt::DirectConnection);
    REQUIRE(invoked == true);
    float after = pm->getLastPos(id, &sc);
    REQUIRE(after == -2.0f);
}
