#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QApplication>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QElapsedTimer>

#include "../src/SoundContainer.h"
#include "../src/WaveformCache.h"
#include "../src/DebugLog.h"

#include <sndfile.h>

static void write_test_tone(const QString& path) {
    // create a 1s 440Hz mono WAV using Python-style approach via libsndfile not available,
    // instead write raw PCM using libsndfile via SF API in C++ would need linking; simpler:
    // Use an existing file if present; test expects /tmp/test_tone.wav created by harness.
    Q_UNUSED(path);
}

TEST_CASE("SoundContainer caching flow writes cache file", "[waveform][cache][integration]") {
    QString path = "/tmp/test_tone.wav";
    REQUIRE(QFile::exists(path));

    // probe header for sampleRate and channels
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE* snd = sf_open(path.toUtf8().constData(), SFM_READ, &sfinfo);
    REQUIRE(snd != nullptr);
    int samplerate = sfinfo.samplerate;
    int channels = sfinfo.channels;
    sf_close(snd);

    int argc = 1;
    char *argv[] = {(char*)"test", nullptr};
    // Install file logging for this test only
    DebugLog::install("/tmp/libresoundboard-debug.log");
    QApplication app(argc, argv);

    SoundContainer sc(nullptr);
    sc.show();
    sc.setFile(path);

    QFileInfo fi(path);
    qint64 size = fi.size();
    qint64 mtime = fi.lastModified().toSecsSinceEpoch();
    int px = qMax(1, 160);
    float dpr = sc.devicePixelRatioF();

    QString cacheDir = WaveformCache::cacheDirPath();
    QDir d(cacheDir);

    // Wait up to 10 seconds for any cache png to appear
    QElapsedTimer et; et.start();
    bool found = false;
    while (et.elapsed() < 10000) {
        QCoreApplication::processEvents();
        QStringList files = d.entryList(QStringList() << "*.png", QDir::Files | QDir::NoDotAndDotDot);
        if (!files.isEmpty()) { found = true; break; }
        QThread::msleep(100);
    }

    DebugLog::uninstall();
    REQUIRE(found == true);
}
