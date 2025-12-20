#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <QTemporaryFile>
#include <thread>
#include <chrono>
#include <sndfile.h>
#include "../src/WaveformWorker.h"

// Helper: write a mono WAV file with given frames and sample-rate (float samples)
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
    // Convert floats to 16-bit signed
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

TEST_CASE("WaveformWorker decode simple wav") {
    // Create temp file
    QTemporaryFile tf;
    tf.setAutoRemove(false);
    REQUIRE(tf.open());
    QString path = tf.fileName();
    tf.close();

    int sr = 8000;
    int channels = 1;
    int frames = 800; // 0.1s
    std::vector<float> samples(frames * channels);
    // fill with ramp 0..0.5
    for (int i = 0; i < frames; ++i) {
        samples[i] = static_cast<float>(i) / static_cast<float>(frames) * 0.5f;
    }
    REQUIRE(write_test_wav(path, sr, channels, samples));

    WaveformResult res = WaveformWorker::decodeFile(path, 160, 1.0);
    REQUIRE(res.sampleRate == sr);
    REQUIRE(res.channels == channels);
    REQUIRE(res.duration == Approx(static_cast<double>(frames) / sr));
    REQUIRE(res.min.size() == 160);
    REQUIRE(res.max.size() == 160);

    // Clean up
    QFile::remove(path);
}

TEST_CASE("WaveformWorker cancellation returns quickly") {
    QTemporaryFile tf;
    tf.setAutoRemove(false);
    REQUIRE(tf.open());
    QString path = tf.fileName();
    tf.close();

    int sr = 44100;
    int channels = 1;
    int frames = 480000; // ~10.9s
    std::vector<float> samples(frames * channels);
    for (int i = 0; i < frames; ++i) samples[i] = 0.0f; // silence
    REQUIRE(write_test_wav(path, sr, channels, samples));

    auto token = QSharedPointer<QAtomicInteger<int>>::create(0);

    auto start = std::chrono::steady_clock::now();
    std::thread t([&](){
        WaveformResult r = WaveformWorker::decodeFile(path, 800, 1.0, token);
        // If cancellation worked, decodeFile should return an empty result (we use sampleRate==0 to indicate cancelled)
        (void)r;
    });

    // Wait a short time then request cancel
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    token->storeRelaxed(1);

    t.join();
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Expect cancellation to return quickly (<100ms)
    REQUIRE(ms < 100);

    QFile::remove(path);
}

TEST_CASE("WaveformWorker base accumulation matches expectations") {
    QTemporaryFile tf;
    tf.setAutoRemove(false);
    REQUIRE(tf.open());
    QString path = tf.fileName();
    tf.close();

    int sr = 8000;
    int channels = 1;
    int frames = 8;
    std::vector<float> samples = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, -0.7f, 0.8f, -0.1f};
    REQUIRE(write_test_wav(path, sr, channels, samples));

    // Request 4 pixels -> 2 frames per bucket
    WaveformResult res = WaveformWorker::decodeFile(path, 4, 1.0);
    REQUIRE(res.sampleRate == sr);
    REQUIRE(res.channels == channels);
    REQUIRE(res.min.size() == 4);
    REQUIRE(res.max.size() == 4);

    // Expected per-bucket min/max (downmixed and using abs)
    std::vector<float> expMin = {-0.2f, -0.4f, -0.7f, -0.8f};
    std::vector<float> expMax = {0.2f, 0.4f, 0.7f, 0.8f};

    for (size_t i = 0; i < expMin.size(); ++i) {
        REQUIRE(res.min[i] == Approx(expMin[i]).margin(1e-3));
        REQUIRE(res.max[i] == Approx(expMax[i]).margin(1e-3));
    }

    QFile::remove(path);
}
