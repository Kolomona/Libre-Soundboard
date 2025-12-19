#include "AudioFile.h"

#include <QFile>
#include <cstring>
#include <cstdlib>
#include <sndfile.h>
#include <mpg123.h>
#include <iostream>

AudioFile::AudioFile(const QString& path)
    : m_path(path)
{
}

bool AudioFile::load(const QString& path)
{
    if (!QFile::exists(path))
        return false;
    m_path = path;
    return true;
}

bool AudioFile::readAllSamples(std::vector<float>& outSamples, int& sampleRate, int& channels) const
{
    if (m_path.isEmpty())
        return false;

    QString p = m_path;
    QByteArray ba = p.toLocal8Bit();
    const char* cpath = ba.constData();

    // Try libsndfile first (handles WAV, FLAC, OGG if compiled)
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    SNDFILE* snd = sf_open(cpath, SFM_READ, &sfinfo);
    if (snd) {
        sampleRate = sfinfo.samplerate;
        channels = sfinfo.channels;
        sf_count_t frames = sfinfo.frames;
        outSamples.resize(frames * channels);
        sf_count_t readcount = sf_readf_float(snd, outSamples.data(), frames);
        sf_close(snd);
        // Diagnostic log
        std::cerr << "AudioFile: libsndfile decoded " << readcount << " frames @ " << sampleRate << " Hz, " << channels << " channels\n";
        return readcount == frames;
    }

    // Fallback to mpg123 for mp3
    mpg123_handle* mh = nullptr;
    if (mpg123_init() == MPG123_OK) {
        int err = 0;
        mh = mpg123_new(NULL, &err);
        if (mh) {
            if (mpg123_open(mh, cpath) == MPG123_OK) {
                long rate = 0;
                int enc = 0;
                int chs = 0;
                mpg123_getformat(mh, &rate, &chs, &enc);
                // Ensure floating point output
                mpg123_format_none(mh);
                mpg123_format(mh, rate, chs, MPG123_ENC_FLOAT_32);

                sampleRate = static_cast<int>(rate);
                channels = chs;

                // Query the negotiated output format
                long rate2 = 0;
                int chs2 = 0;
                int enc2 = 0;
                mpg123_getformat(mh, &rate2, &chs2, &enc2);
                std::cerr << "AudioFile: mpg123 requested rate=" << rate << " chs=" << chs << "; negotiated rate=" << rate2 << " chs=" << chs2 << " enc=" << enc2 << "\n";

                off_t samples = mpg123_length(mh);
                std::vector<unsigned char> raw;
                size_t buffer_size = 16384 * chs * sizeof(float);
                unsigned char* buffer = (unsigned char*)malloc(buffer_size);
                size_t done = 0;
                int r = 0;
                while (true) {
                    r = mpg123_read(mh, buffer, buffer_size, &done);
                    if (done > 0) {
                        raw.insert(raw.end(), buffer, buffer + done);
                    }
                    if (r == MPG123_OK) continue;
                    if (r == MPG123_DONE) break;
                    // error
                    break;
                }
                free(buffer);
                mpg123_close(mh);
                mpg123_delete(mh);
                mpg123_exit();

                if (raw.empty()) {
                    return false;
                }

                std::vector<float> tmp;

                // If mpg123 produced 32-bit floats
                if (enc2 & MPG123_ENC_FLOAT_32) {
                    size_t nfloats = raw.size() / sizeof(float);
                    tmp.resize(nfloats);
                    std::memcpy(tmp.data(), raw.data(), nfloats * sizeof(float));
                } else if (enc2 & MPG123_ENC_SIGNED_16) {
                    // Convert 16-bit signed ints to floats
                    size_t nsamples = raw.size() / sizeof(short);
                    tmp.resize(nsamples);
                    const short* sdata = reinterpret_cast<const short*>(raw.data());
                    for (size_t i = 0; i < nsamples; ++i) {
                        tmp[i] = static_cast<float>(sdata[i]) / 32768.0f;
                    }
                } else {
                    // Unknown encoding: attempt best-effort reinterpret as floats
                    size_t nfloats = raw.size() / sizeof(float);
                    tmp.resize(nfloats);
                    std::memcpy(tmp.data(), raw.data(), nfloats * sizeof(float));
                }

                // Diagnostic log for mpg123 decode
                std::cerr << "AudioFile: mpg123 raw bytes=" << raw.size() << " floats=" << tmp.size() << " frames=" << (tmp.size() / channels) << " @ " << sampleRate << " Hz, " << channels << " channels\n";
                outSamples.swap(tmp);
                return !outSamples.empty();
            }
            mpg123_delete(mh);
        }
        mpg123_exit();
    }

    return false;
}
