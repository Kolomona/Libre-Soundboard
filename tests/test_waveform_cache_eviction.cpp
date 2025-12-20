#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>

#include "../src/WaveformCache.h"

TEST_CASE("WaveformCache eviction reduces total size", "[waveform][cache][evict]") {
    // Make a unique temp cache dir
    QString tmpBase = QDir::tempPath() + QString("/libresoundboard_cache_test_%1").arg(QCoreApplication::applicationPid());
    qputenv("LIBRE_WAVEFORM_CACHE_DIR", tmpBase.toUtf8());

    QString cacheDir = WaveformCache::cacheDirPath();
    REQUIRE(!cacheDir.isEmpty());

    // Ensure starting clean
    WaveformCache::clearAll();

    // Create several cache entries of increasing size
    int count = 8;
    for (int i = 0; i < count; ++i) {
        int w = 50 + i * 40;
        int h = 8 + i;
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(QColor::fromRgb((i * 37) & 0xff, (i * 73) & 0xff, (i * 19) & 0xff));
        QString key = WaveformCache::makeKey(QString("/tmp/fake%1.wav").arg(i), 100 + i, 1234 + i, 1, 44100, 1.0f, w);
        QJsonObject meta;
        meta["path"] = QString("/tmp/fake%1.wav").arg(i);
        meta["size"] = 100 + i;
        meta["mtime"] = 1234 + i;
        meta["channels"] = 1;
        meta["samplerate"] = 44100;
        meta["dpr"] = 1.0f;
        meta["pixelWidth"] = w;
        bool ok = WaveformCache::write(key, img, meta);
        REQUIRE(ok == true);
        // small sleep to ensure distinct file mtimes
        QThread::msleep(10);
    }

    // Compute total size
    QDir d(cacheDir);
    qint64 total = 0;
    for (const QString& f : d.entryList(QStringList() << "*.png" << "*.json", QDir::Files)) {
        total += QFileInfo(d.filePath(f)).size();
    }
    REQUIRE(total > 0);

    // Evict to half the size
    qint64 limit = total / 2;
    WaveformCache::evict(limit, 365);

    qint64 after = 0;
    for (const QString& f : d.entryList(QStringList() << "*.png" << "*.json", QDir::Files)) {
        after += QFileInfo(d.filePath(f)).size();
    }

    REQUIRE(after <= limit);

    // cleanup
    WaveformCache::clearAll();
}
