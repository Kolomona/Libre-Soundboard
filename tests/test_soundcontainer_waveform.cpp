#include <catch2/catch.hpp>
#include <QApplication>
#include <QTemporaryFile>
#include <QFile>
#include <QLabel>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <thread>
#include <chrono>
#include <sndfile.h>

#include "../src/SoundContainer.h"

// Helper: write a small WAV file with given frames and sample-rate (float samples)
static bool write_test_wav(const QString& path, int sampleRate, int channels, const std::vector<float>& samples) {
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = sampleRate;
    sfinfo.channels = channels;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    QByteArray ba = path.toLocal8Bit();
    const char* cpath = ba.constData();
    SNDFILE* snd = sf_open(cpath, SFM_WRITE, &sfinfo);
    if (!snd) return false;
    std::vector<short> buf(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        float v = samples[i];
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        buf[i] = static_cast<short>(v * 32767.0f);
    }
    sf_count_t frames = samples.size() / channels;
    sf_count_t written = sf_writef_short(snd, buf.data(), frames);
    sf_close(snd);
    return written == frames;
}

TEST_CASE("SoundContainer receives waveform pixmap on job completion") {
    // Ensure we can run without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    int argc = 1;
    char arg0[] = "test";
    char* argv[] = { arg0 };
    QApplication app(argc, argv);

    QTemporaryFile tf;
    tf.setAutoRemove(false);
    REQUIRE(tf.open());
    QString path = tf.fileName();
    tf.close();

    const int sr = 8000;
    const int channels = 1;
    const int frames = 800; // short
    std::vector<float> samples(frames * channels);
    for (int i = 0; i < frames; ++i) samples[i] = static_cast<float>(i) / static_cast<float>(frames) * 0.5f;
    REQUIRE(write_test_wav(path, sr, channels, samples));

    SoundContainer* sc = new SoundContainer(nullptr);
    sc->show();
    sc->setFile(path);

    // Wait for up to 2s for the waveform pixmap to be set by the worker
    QElapsedTimer t; t.start();
    QLabel* waveformLabel = nullptr;
    while (t.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        // Find any QLabel child that has a non-null pixmap
        auto labels = sc->findChildren<QLabel*>();
        for (QLabel* l : labels) {
            const QPixmap* pm = l->pixmap();
            if (pm && !pm->isNull()) {
                waveformLabel = l;
                break;
            }
        }
        if (waveformLabel) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    REQUIRE(waveformLabel != nullptr);

    // Ensure filename label still contains the filename text/tooltip
    QString fname = QFileInfo(path).fileName();
    bool foundFilenameLabel = false;
    for (QLabel* l : sc->findChildren<QLabel*>()) {
        if (l->toolTip() == fname || l->text() == fname) {
            foundFilenameLabel = true;
            break;
        }
    }
    REQUIRE(foundFilenameLabel);

    delete sc;
    QFile::remove(path);
}
