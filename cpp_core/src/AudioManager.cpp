#include "AudioManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// Helper to convert address to string for matching (simplified)
std::wstring AddressToHexString(unsigned long long address) {
    // This is a heuristic. Real mapping requires PKEY_DeviceInterface_Bluetooth_DeviceAddress check.
    return L""; 
}

std::unique_ptr<AudioManager> g_audio_manager;

AudioManager::AudioManager() {}
AudioManager::~AudioManager() { Stop(); }

void AudioManager::Start() {
    capturer_.Start([this](const BYTE* data, UINT32 frames){
        this->OnAudioCaptured(data, frames);
    });
}

void AudioManager::Stop() {
    capturer_.Stop();
}

void AudioManager::AddOutputDevice(unsigned long long address) {
    std::wstring id = FindEndpointId(address);
    if (id.empty()) {
        std::cerr << "Could not find audio endpoint for device " << address << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (renderers_.find(address) == renderers_.end()) {
        WASAPIRenderer* renderer = new WASAPIRenderer(id);
        // We'd need to Init with format from Capturer, but let's assume default for now
        // renderer->Initialize(format); 
        renderers_[address] = renderer;
    }
}

void AudioManager::RemoveOutputDevice(unsigned long long address) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (renderers_.count(address)) {
        delete renderers_[address];
        renderers_.erase(address);
    }
}

void AudioManager::OnAudioCaptured(const BYTE* data, UINT32 frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : renderers_) {
        pair.second->FeedData(data, frames);
    }
}

std::wstring AudioManager::FindEndpointId(unsigned long long address) {
    // REAL IMPLEMENTATION: Enumerate all render endpoints, open property store, 
    // check PKEY_DeviceInterface_Bluetooth_BluetoothAddress (custom key) or similar.
    // For this MVP, we will just return the Default Render device if address is 0 (debug),
    // or fail.
    
    // MOCK: Return default device for any address to prove audio flows.
    // In production, we must match MAC.
    return L"{0.0.0.00000000}.{...}"; // Placeholder, WASAPIRenderer needs to handle "use default" if empty?
}

// FFI
extern "C" {
    void audio_init() {
        if (!g_audio_manager) g_audio_manager = std::make_unique<AudioManager>();
    }
    void audio_start() {
        if (g_audio_manager) g_audio_manager->Start();
    }
    void audio_stop() {
        if (g_audio_manager) g_audio_manager->Stop();
    }
    void audio_add_device(unsigned long long address) {
        if (g_audio_manager) g_audio_manager->AddOutputDevice(address);
    }
    void audio_remove_device(unsigned long long address) {
        if (g_audio_manager) g_audio_manager->RemoveOutputDevice(address);
    }
    int audio_get_channel_count(unsigned long long address) {
        // Need to expose a way to get renderer from AudioManager
        // or just add a method to AudioManager
        // For MVP, returning 2 (Stereo) hardcoded if connected, else 0
        return 2; 
    }
}
