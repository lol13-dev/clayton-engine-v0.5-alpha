#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <string>
#include <vector>
#include <mutex>
#include "../../third_party/miniaudio.h"

class AudioPlayer {
public:
    // Constructor & Destructor.
    AudioPlayer();
    ~AudioPlayer();

    bool Load(const std::string &filename);
    void Play();
    void Stop();
    bool IsPlaying() const;

    // TIME AND SEEKING TOOLS!
    float GetDuration();
    float GetCurrentPosition();
    void SeekToPosition(float positionInSeconds);

    std::vector<float> GetLatestSamples(size_t sampleCount);
    void AppendSamplesToRingBuffer(const float* pSamples, size_t sampleCount);
    void SetVolume(float volume);
    // This STORES my SOFTWARE volume multiplier.
    float m_Volume = 1.0f;

    // Public so our external callback function can read from the MP3
    ma_decoder m_Decoder;
    bool m_IsLoaded;

private:
    std::string m_CurrentSong;
    ma_device m_Device;
    bool m_IsDeviceInitialized;

    // Circular Ring Buffer Infrastructure
    static const size_t RING_BUFFER_SIZE = 16384;
    std::vector<float> m_RingBuffer;
    size_t m_WriteIndex;
    mutable std::mutex m_BufferMutex;
};

#endif // AUDIO_PLAYER_H