#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Audio/AudioStructs.h"
#include <string>

/// <summary>
/// 音声再生を担うコンポーネント (UnityライクなAudioSource)
/// </summary>
class AudioSourceComponent : public IComponent {
public:
    AudioSourceComponent();
    ~AudioSourceComponent() override;

    void Initialize() override;
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "AudioSource"; }

    // ==========================================
    // 再生制御
    // ==========================================
    void Load(const std::string& filePath);
    void Play();
    void Stop();

    // ==========================================
    // パラメータ操作
    // ==========================================
    const std::string& GetFilePath() const { return filePath_; }
    void SetFilePath(const std::string& path) { filePath_ = path; }

    float GetVolume() const { return volume_; }
    void SetVolume(float volume);

    bool IsLoop() const { return isLoop_; }
    void SetLoop(bool loop) { isLoop_ = loop; }

    bool IsAutoPlay() const { return autoPlay_; }
    void SetAutoPlay(bool autoPlay) { autoPlay_ = autoPlay; }

    bool IsLoaded() const { return isLoaded_; }

private:
    void UnloadSound();

private:
    std::string filePath_ = "";
    float volume_ = 1.0f;
    bool isLoop_ = false;
    bool autoPlay_ = false;

    SoundData soundData_{};
    bool isLoaded_ = false;
    bool hasAutoPlayed_ = false;
};
