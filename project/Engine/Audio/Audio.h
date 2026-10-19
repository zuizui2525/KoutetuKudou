#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <string>
#include <vector>
#include <cassert>
#include "Engine/Audio/AudioStructs.h"

class Audio {
public:
    Audio();
    ~Audio();

    // シングルトンインスタンス取得
    static Audio* GetInstance();

    // 初期化と終了処理
    void Initialize();
    void Finalize();

    // 一括ポーズ / レジューム
    void PauseAll();
    void ResumeAll();
    bool IsPaused() const { return isPaused_; }

    // 音声読み込み (.wav / .mp3対応)
    SoundData LoadSound(const std::string& filePath);

    // 再生・停止・解放
    void PlaySoundW(SoundData& soundData, float volume = 1.0f, bool loop = false);
    void StopSound(SoundData& soundData);
    void Unload(SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    bool isPaused_ = false;
};
