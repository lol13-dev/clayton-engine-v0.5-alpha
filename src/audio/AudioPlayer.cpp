// UPGRADED VERSION. SEE src/audio/AudioPlayer_v1.cpp for the original version without the ring buffer and FFT support.
#define MINIAUDIO_IMPLEMENTATION
#include "../../third_party/miniaudio.h"
#include "AudioPlayer.h"
#include <iostream>

// ==========================================================
// THE INTERCEPTOR CALLBACK
// This runs on a background thread every few milliseconds
// ==========================================================
void AudioDataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    // Unused input (we are only playing back, not recording a microphone)
    (void)pInput;

    AudioPlayer *player = static_cast<AudioPlayer *>(pDevice->pUserData);
    if (player == nullptr || !player->m_IsLoaded)
        return;

    // 1. Read the decoded MP3 audio frames straight into the speaker output buffer
    ma_uint64 framesRead = 0;

    // 2. IMPORTANT: We must pass the memory address of framesRead so miniaudio can write how many frames it actually decoded and sent to the speakers! This is crucial for our ring buffer logic to work correctly.
    ma_result result = ma_decoder_read_pcm_frames(
        &player->m_Decoder,
        pOutput,
        frameCount,
        &framesRead // <-- NEW: Passing the memory address so miniaudio can write the result!
    );

    // 3. Intercept those exact same frames and send them to your FFT Ring Buffer!
    if (result == MA_SUCCESS && framesRead > 0)
    {
        // Multiply by channel count to ensure I copy the entire block of DATA.
        player->AppendSamplesToRingBuffer(
            static_cast<const float *>(pOutput),
            framesRead * pDevice->playback.channels);
    }

    // 4. SOFTWARE VOLUME OVERDRIVE (Add this right after reading frames)
    // Step 1. Cast the raw memory into an array of readable floats
    float *pFloatOutput = (float *)pOutput;

    // Step 2. Calculate the total number of audio samples (Frames * 2 for Stereo)
    size_t totalSamples = framesRead * pDevice->playback.channels;

    // Step 3. Multiply every single sample by our volume slider!
    for (size_t i = 0; i < totalSamples; i++)
    {
        pFloatOutput[i] *= player->m_Volume;
    }
}

// ==========================================================
// CLASS IMPLEMENTATION
// ==========================================================

AudioPlayer::AudioPlayer()
    : m_IsLoaded(false), m_IsDeviceInitialized(false), m_WriteIndex(0)
{
    // Pre-allocate the memory buffer so it doesn't stutter during playback
    m_RingBuffer.resize(RING_BUFFER_SIZE, 0.0f);
}

AudioPlayer::~AudioPlayer()
{
    Stop();
    if (m_IsDeviceInitialized)
        ma_device_uninit(&m_Device);
    if (m_IsLoaded)
        ma_decoder_uninit(&m_Decoder);
}

bool AudioPlayer::Load(const std::string &filename)
{
    // Clean up previously loaded songs
    if (m_IsLoaded)
    {
        ma_decoder_uninit(&m_Decoder);
        m_IsLoaded = false;
    }
    if (m_IsDeviceInitialized)
    {
        ma_device_uninit(&m_Device);
        m_IsDeviceInitialized = false;
    }

    // 1. Initialize the Decoder (Forces Mono channel to make FFT math much easier!)
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 1, 44100);
    ma_result result = ma_decoder_init_file(filename.c_str(), &decoderConfig, &m_Decoder);

    if (result != MA_SUCCESS)
    {
        std::cout << "[ERROR] Could not load audio asset: " << filename << "\n";
        return false;
    }
    m_IsLoaded = true;

    // 2. Initialize the Hardware Device (Connects to Speakers + Attaches our Callback)
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = m_Decoder.outputFormat;
    deviceConfig.playback.channels = m_Decoder.outputChannels;
    deviceConfig.sampleRate = m_Decoder.outputSampleRate;
    deviceConfig.dataCallback = AudioDataCallback; // Connect the function!
    deviceConfig.pUserData = this;                 // Pass the class instance!

    result = ma_device_init(NULL, &deviceConfig, &m_Device);
    if (result != MA_SUCCESS)
    {
        std::cout << "[ERROR] Failed to initialize Mac hardware playback device.\n";
        ma_decoder_uninit(&m_Decoder);
        m_IsLoaded = false;
        return false;
    }
    m_IsDeviceInitialized = true;

    m_CurrentSong = filename;
    std::cout << "[SUCCESS] Loaded asset stream: " << filename << "\n";
    return true;
}

// =====================================
// SET MASTER VOLUME (Supports up to 200%)
// =====================================
void AudioPlayer::SetVolume(float volume)
{
    // [C++ LEARNING] miniaudio uses ma_sound_set_volume.
    // It accepts a float multiplier (1.0 = normal, 0.5 = half, 2.0 = double!)
    // NOTE: Make sure your internal sound object is named 'sound' or 'm_sound'. Update if needed!
    // ma_device_set_master_volume(&m_Device, volume);

    // [C++ LEARNING] We abandon the hardware OS volume because Macs block it.
    // Instead, we save the slider value here so our callback can use it!
    m_Volume = volume;
}

void AudioPlayer::Play()
{
    if (!m_IsDeviceInitialized)
        return;
    ma_device_start(&m_Device);
    std::cout << "[PLAYING] Streaming active channel: " << m_CurrentSong << "\n";
}

void AudioPlayer::Stop()
{
    if (!m_IsDeviceInitialized)
        return;
    ma_device_stop(&m_Device);
    std::cout << "[STOPPED] Channel track handle locked: " << m_CurrentSong << "\n";
}

bool AudioPlayer::IsPlaying() const
{
    if (!m_IsDeviceInitialized)
        return false;
    return ma_device_is_started(&m_Device);
}

// ==========================================================
// RING BUFFER LOGIC (THREAD SAFE)
// ==========================================================

void AudioPlayer::AppendSamplesToRingBuffer(const float *pSamples, size_t sampleCount)
{
    std::lock_guard<std::mutex> lock(m_BufferMutex);

    for (size_t i = 0; i < sampleCount; ++i)
    {
        m_RingBuffer[m_WriteIndex] = pSamples[i];
        m_WriteIndex = (m_WriteIndex + 1) % RING_BUFFER_SIZE;
    }
}

// ==========================================================
// TRACK PROGESS & SEEKING
// ==========================================================
float AudioPlayer::GetDuration()
{
    if (!m_IsLoaded)
        return 0.0f;
    ma_uint64 lengthInFrames;
    if (ma_decoder_get_length_in_pcm_frames(&m_Decoder, &lengthInFrames) == MA_SUCCESS)
    {
        // TOTAL FRAMES / SAMPLE RATE = Total SECONDS.
        return (float)lengthInFrames / (float)m_Decoder.outputSampleRate;
    }
    return 0.0f;
}

float AudioPlayer::GetCurrentPosition()
{
    if (!m_IsLoaded)
        return 0.0f;
    ma_uint64 cursorInFrames;
    if (ma_decoder_get_cursor_in_pcm_frames(&m_Decoder, &cursorInFrames) == MA_SUCCESS)
    {
        // CURRENT FRAME / SAMPLE RATE = Current SECONDS.
        return (float)cursorInFrames / (float)m_Decoder.outputSampleRate;
    }
    return 0.0f;
}

void AudioPlayer::SeekToPosition(float positionInSeconds)
{
    if (!m_IsLoaded)
        return;
    // TARGET SECOND * SAMPLE RATE = Target Frame.
    ma_uint64 targetFrame = (ma_uint64)(positionInSeconds * (float)m_Decoder.outputSampleRate);
    ma_decoder_seek_to_pcm_frame(&m_Decoder, targetFrame);
}

std::vector<float> AudioPlayer::GetLatestSamples(size_t sampleCount)
{
    std::lock_guard<std::mutex> lock(m_BufferMutex);
    std::vector<float> outputSamples(sampleCount, 0.0f);

    long long readStart = static_cast<long long>(m_WriteIndex) - static_cast<long long>(sampleCount);
    if (readStart < 0)
    {
        readStart += RING_BUFFER_SIZE;
    }

    size_t currentReadIndex = static_cast<size_t>(readStart);
    for (size_t i = 0; i < sampleCount; ++i)
    {
        outputSamples[i] = m_RingBuffer[currentReadIndex];
        currentReadIndex = (currentReadIndex + 1) % RING_BUFFER_SIZE;
    }

    return outputSamples;
}