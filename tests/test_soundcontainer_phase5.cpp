#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QLabel>
#include <QFileInfo>

#include "../src/SoundContainer.h"
#include "../src/WaveformWorker.h" // for WaveformResult

static QVector<float> vec(std::initializer_list<float> l) { return QVector<float>(l); }

TEST_CASE("SoundContainer receives waveform result and preserves filename/tooltip", "[waveform][phase5]") {
    int argc = 0;
    char** argv = nullptr;
    QApplication app(argc, argv);

    SoundContainer sc(nullptr);

    const QString path = "/tmp/dummy_test_sound.wav";
    sc.setFile(path);

    // Build a small deterministic waveform result
    WaveformResult res;
    res.channels = 1;
    res.sampleRate = 44100;
    res.duration = 1.0; // seconds
    // create simple min/max arrays
    QVector<float> mins, maxs;
    for (int i = 0; i < 16; ++i) {
        mins.append(-0.2f * (i % 3));
        maxs.append(0.3f * (i % 4));
    }
    res.min = mins;
    res.max = maxs;

    // Inject the result via the test helper (synchronous)
    sc.applyWaveformResultForTest(res);

    // Find the filename label and waveform label among children
    QList<QLabel*> labels = sc.findChildren<QLabel*>();
    QLabel* filenameLabel = nullptr;
    QLabel* waveformLabel = nullptr;
    for (QLabel* l : labels) {
        if (!l->pixmap()) {
            // likely filename label (has text)
            if (l->text() == QFileInfo(path).fileName()) filenameLabel = l;
        } else {
            waveformLabel = l;
        }
    }

    REQUIRE(filenameLabel != nullptr);
    REQUIRE(waveformLabel != nullptr);
    REQUIRE(filenameLabel->toolTip() == QFileInfo(path).fileName());
    REQUIRE(!waveformLabel->pixmap()->isNull());
}
