#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/WaveformRenderer.h"
#include "../src/WaveformCache.h"
#include "../src/WaveformPyramid.h"
#include <QJsonObject>

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
    meta["width"] = cssWidth;
    meta["height"] = cssHeight;
    bool ok = WaveformCache::write(key, img, meta);
    REQUIRE(ok == true);

    QJsonObject out;
    QImage loaded = WaveformCache::load(key, &out);
    REQUIRE(!loaded.isNull());
    REQUIRE(loaded.width() == img.width());
}
