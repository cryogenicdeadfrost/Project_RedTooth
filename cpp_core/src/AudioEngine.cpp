#include "AudioEngine.h"
#include <iostream>
#include <functiondiscoverykeys_devpkey.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

// ================= CAPTURER =================

WASAPICapturer::WASAPICapturer() : enumerator_(NULL), device_(NULL), audio_client_(NULL), capture_client_(NULL), running_(false) {
    CoInitialize(NULL);
    CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator_);
}

WASAPICapturer::~WASAPICapturer() {
    Stop();
    if (enumerator_) enumerator_->Release();
    CoUninitialize();
}

bool WASAPICapturer::Start(std::function<void(const BYTE*, UINT32)> data_callback) {
    if (running_) return false;
    
    HRESULT hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_); // Loopback from render
    if (FAILED(hr)) return false;

    hr = device_->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audio_client_);
    if (FAILED(hr)) return false;

    WAVEFORMATEX* pwfx = NULL;
    audio_client_->GetMixFormat(&pwfx);

    // Initialize for Loopback
    hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, NULL);
    if (FAILED(hr)) return false;

    hr = audio_client_->GetService(IID_IAudioCaptureClient, (void**)&capture_client_);
    if (FAILED(hr)) return false;

    audio_client_->Start();
    
    callback_ = data_callback;
    running_ = true;
    capture_thread_ = std::thread(&WASAPICapturer::CaptureLoop, this);
    
    CoTaskMemFree(pwfx);
    return true;
}

void WASAPICapturer::Stop() {
    running_ = false;
    if (capture_thread_.joinable()) capture_thread_.join();
    if (audio_client_) audio_client_->Stop();
    if (capture_client_) { capture_client_->Release(); capture_client_ = NULL; }
    if (audio_client_) { audio_client_->Release(); audio_client_ = NULL; }
    if (device_) { device_->Release(); device_ = NULL; }
}

void WASAPICapturer::CaptureLoop() {
    CoInitialize(NULL); // Thread needs COM
    UINT32 packetLength = 0;
    while (running_) {
        HRESULT hr = capture_client_->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        while (packetLength != 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;

            hr = capture_client_->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr)) break;

            if (callback_) {
                // Handle flags (silent, etc.)
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    // Send silence
                } else {
                    callback_(pData, numFramesAvailable);
                }
            }

            capture_client_->ReleaseBuffer(numFramesAvailable);
            capture_client_->GetNextPacketSize(&packetLength);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Basic throttling
    }
    CoUninitialize();
}

// ================= RENDERER =================

WASAPIRenderer::WASAPIRenderer(std::wstring device_id) : device_id_(device_id), device_(NULL), audio_client_(NULL), render_client_(NULL), channel_count_(0) {
}

WASAPIRenderer::~WASAPIRenderer() {
    if (render_client_) render_client_->Release();
    if (audio_client_) audio_client_->Release();
    if (device_) device_->Release();
}

bool WASAPIRenderer::Initialize(WAVEFORMATEX* format) {
    IMMDeviceEnumerator* pEnumerator = NULL;
    CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    
    HRESULT hr = pEnumerator->GetDevice(device_id_.c_str(), &device_);
    if (pEnumerator) pEnumerator->Release();
    if (FAILED(hr)) return false;

    hr = device_->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audio_client_);
    if (FAILED(hr)) return false;

    // Use Mix Format to determine device capabilities if format is NULL or we check device pref
    WAVEFORMATEX* mixFormat = NULL;
    audio_client_->GetMixFormat(&mixFormat);
    channel_count_ = mixFormat->nChannels;
    CoTaskMemFree(mixFormat);

    hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000 /* 1 sec */, 0, format, NULL);
    if (FAILED(hr)) return false;

    hr = audio_client_->GetService(IID_IAudioRenderClient, (void**)&render_client_);
    if (FAILED(hr)) return false;

    return SUCCEEDED(audio_client_->Start());
}

void WASAPIRenderer::FeedData(const BYTE* data, UINT32 frames) {
    if (!render_client_) return;
    
    BYTE* pBuffer;
    HRESULT hr = render_client_->GetBuffer(frames, &pBuffer);
    if (SUCCEEDED(hr)) {
        // Assume format matches for MVP. Real code needs resampling/conversion.
        // Copy data. size = frames * frame_size. 
        // We lack frame_size here in this simplified snippet, assuming stereo float (8 bytes/frame) for default loopback.
        memcpy(pBuffer, data, frames * 8); 
        render_client_->ReleaseBuffer(frames, 0);
    }
}
