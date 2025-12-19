#pragma once

#include <QString>

/**
 * AudioFile helper — stub for loading metadata and paths.
 */
class AudioFile
{
public:
    AudioFile() = default;
    explicit AudioFile(const QString& path);

    bool load(const QString& path);

    /**
     * Read all samples into a float vector (interleaved).
     * Returns true on success and fills out sampleRate and channels.
     */
    bool readAllSamples(std::vector<float>& outSamples, int& sampleRate, int& channels) const;

    QString path() const { return m_path; }

private:
    QString m_path;
};
