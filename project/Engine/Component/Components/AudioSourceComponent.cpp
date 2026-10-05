#include "Engine/Component/Components/AudioSourceComponent.h"
#include "Engine/Audio/Audio.h"
#include <filesystem>
#include <memory>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // 共有Audioエンジンインスタンス
    std::unique_ptr<Audio> gSharedAudioEngine = nullptr;

    Audio* GetSharedAudioEngine() {
        if (!gSharedAudioEngine) {
            gSharedAudioEngine = std::make_unique<Audio>();
            gSharedAudioEngine->Initialize();
        }
        return gSharedAudioEngine.get();
    }

    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr size_t kPathBufferSize = 256;
    constexpr float kMinVolume = 0.0f;
    constexpr float kMaxVolume = 1.0f;
}

AudioSourceComponent::AudioSourceComponent() = default;

AudioSourceComponent::~AudioSourceComponent() {
    Stop();
    UnloadSound();
}

void AudioSourceComponent::Initialize() {
    if (!filePath_.empty() && !isLoaded_) {
        Load(filePath_);
    }
}

void AudioSourceComponent::Update() {
    if (!isActive_) return;

    // 自動再生処理 (1度のみ実行)
    if (autoPlay_ && !hasAutoPlayed_ && isLoaded_) {
        Play();
        hasAutoPlayed_ = true;
    }
}

void AudioSourceComponent::Load(const std::string& filePath) {
    UnloadSound();
    filePath_ = filePath;

    if (filePath_.empty()) return;

    if (!std::filesystem::exists(filePath_)) {
        return;
    }

    Audio* audio = GetSharedAudioEngine();
    if (audio) {
        soundData_ = audio->LoadSound(filePath_);
        isLoaded_ = (soundData_.wfex != nullptr);
    }
}

void AudioSourceComponent::Play() {
    if (!isLoaded_) {
        if (!filePath_.empty()) {
            Load(filePath_);
        }
    }

    if (isLoaded_) {
        Audio* audio = GetSharedAudioEngine();
        if (audio) {
            audio->PlaySoundW(soundData_, volume_, isLoop_);
        }
    }
}

void AudioSourceComponent::Stop() {
    if (isLoaded_) {
        Audio* audio = GetSharedAudioEngine();
        if (audio) {
            audio->StopSound(soundData_);
        }
    }
}

void AudioSourceComponent::UnloadSound() {
    if (isLoaded_) {
        Audio* audio = GetSharedAudioEngine();
        if (audio) {
            audio->Unload(soundData_);
        }
        isLoaded_ = false;
        hasAutoPlayed_ = false;
    }
}

void AudioSourceComponent::SetVolume(float volume) {
    volume_ = std::clamp(volume, kMinVolume, kMaxVolume);
    if (isLoaded_ && soundData_.sourceVoice) {
        soundData_.sourceVoice->SetVolume(volume_);
    }
}

void AudioSourceComponent::DrawInspector() {
#ifdef _USEIMGUI
    // 1. ファイルパス入力
    char pathBuf[kPathBufferSize]{};
    strncpy_s(pathBuf, filePath_.c_str(), sizeof(pathBuf) - 1);
    if (ImGui::InputText("Audio Path##AudioSource", pathBuf, sizeof(pathBuf))) {
        filePath_ = pathBuf;
    }

    ImGui::SameLine();
    if (ImGui::Button("Load##AudioSource")) {
        Load(filePath_);
    }

    // 2. 音量スライダー
    float vol = volume_;
    if (ImGui::SliderFloat("Volume##AudioSource", &vol, kMinVolume, kMaxVolume, "%.2f")) {
        SetVolume(vol);
    }

    // 3. ループ
    bool loop = isLoop_;
    if (ImGui::Checkbox("Loop##AudioSource", &loop)) {
        isLoop_ = loop;
    }

    // 4. 自動再生
    bool autoPlay = autoPlay_;
    if (ImGui::Checkbox("Auto Play##AudioSource", &autoPlay)) {
        autoPlay_ = autoPlay;
    }

    // 5. テスト再生 / 停止 ボタン
    if (ImGui::Button("Play Sound##AudioSource")) {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop Sound##AudioSource")) {
        Stop();
    }
#endif
}
