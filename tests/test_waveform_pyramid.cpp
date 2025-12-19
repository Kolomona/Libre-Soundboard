#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/WaveformPyramid.h"

// Helper to flatten samples for mono
static QVector<float> vec(std::initializer_list<float> l) {
    return QVector<float>(l);
}

TEST_CASE("pyramid mono small buffer", "[waveform][pyramid]") {
    // 6 frames, mono
    QVector<float> samples = vec({-1.0f, -0.5f, 0.2f, 0.7f, -0.3f, 0.1f});
    int channels = 1;
    int baseBucket = 2; // 2 frames per bucket

    auto levels = WaveformPyramid::build(samples, channels, baseBucket);
    REQUIRE(levels.size() >= 2);

    // Level 0: three buckets
    REQUIRE(levels[0].min.size() == 3);
    REQUIRE(levels[0].max.size() == 3);
    REQUIRE(levels[0].min[0] == Approx(-1.0f));
    REQUIRE(levels[0].max[0] == Approx(-0.5f));
    REQUIRE(levels[0].min[1] == Approx(0.2f));
    REQUIRE(levels[0].max[1] == Approx(0.7f));
    REQUIRE(levels[0].min[2] == Approx(-0.3f));
    REQUIRE(levels[0].max[2] == Approx(0.1f));

    // Level 1: combine pairs -> two buckets
    REQUIRE(levels[1].min.size() == 2);
    REQUIRE(levels[1].max.size() == 2);
    REQUIRE(levels[1].min[0] == Approx(-1.0f));
    REQUIRE(levels[1].max[0] == Approx(0.7f));
    REQUIRE(levels[1].min[1] == Approx(-0.3f));
    REQUIRE(levels[1].max[1] == Approx(0.1f));
}

TEST_CASE("pyramid stereo interleaved", "[waveform][pyramid]") {
    // 4 frames, stereo interleaved (L,R)
    // frames: (0.1, -0.1), (0.5, -0.2), (-0.3, 0.3), (0.2, 0.4)
    QVector<float> samples = vec({0.1f, -0.1f, 0.5f, -0.2f, -0.3f, 0.3f, 0.2f, 0.4f});
    int channels = 2;
    int baseBucket = 2; // 2 frames per bucket

    auto levels = WaveformPyramid::build(samples, channels, baseBucket);
    REQUIRE(levels.size() >= 1);
    // Level 0: two buckets (each bucket includes 2 frames * 2 channels = 4 samples)
    REQUIRE(levels[0].min.size() == 2);
    REQUIRE(levels[0].max.size() == 2);

    // Bucket 0 values: frames 0 and 1
    // values: 0.1, -0.1, 0.5, -0.2 -> min -0.2, max 0.5
    REQUIRE(levels[0].min[0] == Approx(-0.2f));
    REQUIRE(levels[0].max[0] == Approx(0.5f));

    // Bucket 1 values: frames 2 and 3 -> -0.3,0.3,0.2,0.4 -> min -0.3 max 0.4
    REQUIRE(levels[0].min[1] == Approx(-0.3f));
    REQUIRE(levels[0].max[1] == Approx(0.4f));
}

TEST_CASE("select level for pixel width", "[waveform][pyramid][select]") {
    int totalFrames = 48000; // e.g., 1s @48k
    int baseBucket = 256;
    int maxLevels = 10;

    // If desired pixels equals ceil(frames/baseBucket) -> should pick level 0
    int desiredPixels = static_cast<int>(std::ceil(static_cast<double>(totalFrames) / static_cast<double>(baseBucket)));
    int level = WaveformPyramid::selectLevelForPixelWidth(totalFrames, baseBucket, desiredPixels, maxLevels);
    REQUIRE(level == 0);

    // If desired pixels very small (1), expect highest level (bucket size large)
    int levelSmall = WaveformPyramid::selectLevelForPixelWidth(totalFrames, baseBucket, 1, maxLevels);
    REQUIRE(levelSmall > 0);
}
