#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

class WASAPICapturer {
public:
    WASAPICapturer();
    ~WASAPICapturer();
    
    bool Start(std::function<void(const BYTE*, UINT32)> data_callback);
    void Stop();

private:
    void CaptureLoop();
    
    IMMDeviceEnumerator* enumerator_;
    IMMDevice* device_;
    IAudioClient* audio_client_;
    IAudioCaptureClient* capture_client_;
    std::atomic<bool> running_;
    std::thread capture_thread_;
    std::function<void(const BYTE*, UINT32)> callback_;
};

class WASAPIRenderer {
public:
    WASAPIRenderer(std::wstring device_id);
    ~WASAPIRenderer();

    bool Initialize(WAVEFORMATEX* format);
    void FeedData(const BYTE* data, UINT32 frames);
    int GetChannelCount() const { return channel_count_; }

private:
    std::wstring device_id_;
    IMMDevice* device_;
    IAudioClient* audio_client_;
    IAudioRenderClient* render_client_;
    UINT32 buffer_size_;
    int channel_count_;
};
