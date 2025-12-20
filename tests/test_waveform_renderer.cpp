#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/WaveformRenderer.h"
#include "../src/WaveformCache.h"
#include "../src/WaveformPyramid.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QIODevice>

static QVector<float> vec(std::initializer_list<float> l) { return QVector<float>(l); }

TEST_CASE("render level to image and cache", "[waveform][renderer][cache]") {
    QVector<float> samples = vec({-1.0f, -0.5f, 0.2f, 0.7f, -0.3f, 0.1f});
    int channels = 1;
    int baseBucket = 2;
    auto levels = WaveformPyramid::build(samples, channels, baseBucket);
    REQUIRE(levels.size() >= 1);

    int cssWidth = 80;
    float dpr = 1.0f;
    int cssHeight = 16;
    QImage img = Waveform::renderLevelToImage(levels[0], cssWidth, dpr, cssHeight);
    REQUIRE(!img.isNull());
    REQUIRE((img.width() == cssWidth * static_cast<int>(std::ceil(dpr)) || img.width() == cssWidth));
    REQUIRE((img.height() == cssHeight * static_cast<int>(std::ceil(dpr)) || img.height() == cssHeight));

    QString key = WaveformCache::makeKey("dummy.wav", 1234, 5678, channels, 44100, dpr, cssWidth);
    QJsonObject meta;
    meta["path"] = "dummy.wav";
    meta["size"] = 1234;
    meta["mtime"] = 5678;
    meta["channels"] = channels;
    meta["samplerate"] = 44100;
    meta["dpr"] = dpr;
    meta["pixelWidth"] = cssWidth;
    meta["width"] = cssWidth;
    meta["height"] = cssHeight;
    bool ok = WaveformCache::write(key, img, meta);
    REQUIRE(ok == true);

    // For debugging: print metadata file contents written to disk
    QString dir = WaveformCache::cacheDirPath();
    QDir d(dir);
    QString metaPath = d.filePath(key + ".json");
    if (QFile::exists(metaPath)) {
        QFile mf(metaPath);
        if (mf.open(QIODevice::ReadOnly)) {
            QByteArray mb = mf.readAll();
            mf.close();
            INFO("metadata file: " + QString::fromUtf8(mb));
        }
    }

    QJsonObject out;
    QImage loaded = WaveformCache::load(key, &out);
    REQUIRE(!loaded.isNull());
    REQUIRE(loaded.width() == img.width());

    // Compare a few key pixel columns to ensure deterministic rendering persisted
    auto samplePixel = [&](const QImage& a, int x) -> QRgb {
        if (x < 0) x = 0;
        if (x >= a.width()) x = a.width() - 1;
        return a.pixel(x, a.height() / 2);
    };

    QRgb p0 = samplePixel(img, 0);
    QRgb pm = samplePixel(img, img.width() / 2);
    QRgb pe = samplePixel(img, img.width() - 1);

    QRgb l0 = samplePixel(loaded, 0);
    QRgb lm = samplePixel(loaded, loaded.width() / 2);
    QRgb le = samplePixel(loaded, loaded.width() - 1);

    REQUIRE(p0 == l0);
    REQUIRE(pm == lm);
    REQUIRE(pe == le);

    // Now simulate metadata mismatch -> cache should be rejected and files removed
    QString dir = WaveformCache::cacheDirPath();
    QDir d(dir);
    QString metaPath = d.filePath(key + ".json");
    REQUIRE(QFile::exists(metaPath));

    // Load and mutate metadata
    QFile mfile(metaPath);
    REQUIRE(mfile.open(QIODevice::ReadWrite));
    QByteArray metaBytes = mfile.readAll();
    mfile.resize(0);
    QJsonDocument md = QJsonDocument::fromJson(metaBytes);
    QJsonObject mobj = md.isObject() ? md.object() : QJsonObject();
    // change mtime to a different value
    mobj["mtime"] = QJsonValue::fromVariant(QVariant::fromValue<double>(mobj.value("mtime").toDouble() + 1.0));
    QByteArray newb = QJsonDocument(mobj).toJson(QJsonDocument::Compact);
    mfile.write(newb);
    mfile.close();

    QJsonObject out2;
    QImage loaded2 = WaveformCache::load(key, &out2);
    REQUIRE(loaded2.isNull());
    // files should be removed on mismatch
    QString imgPath = d.filePath(key + ".png");
    REQUIRE(!QFile::exists(imgPath));
    REQUIRE(!QFile::exists(metaPath));
}
