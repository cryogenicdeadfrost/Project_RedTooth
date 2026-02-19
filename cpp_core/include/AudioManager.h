#pragma once
#include "AudioEngine.h"
#include <map>

// Manager to handle routing
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    void Start();
    void Stop();

    void AddOutputDevice(unsigned long long bt_address);
    void RemoveOutputDevice(unsigned long long bt_address);

private:
    void OnAudioCaptured(const BYTE* data, UINT32 frames);
    std::wstring FindEndpointId(unsigned long long address);

    WASAPICapturer capturer_;
    std::map<unsigned long long, WASAPIRenderer*> renderers_;
    std::mutex mutex_;
};

// C API
extern "C" {
    void audio_init();
    void audio_start();
    void audio_stop();
    void audio_add_device(unsigned long long address);
    void audio_remove_device(unsigned long long address);
    int audio_get_channel_count(unsigned long long address);
}
