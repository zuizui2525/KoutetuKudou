#include "Engine/Audio/Audio.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Base/Log/Log.h"
#include <iostream>
#include <chrono>
#include <format>
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "XAudio2.lib")

Audio* Audio::GetInstance() {
    static Audio instance;
    if (!instance.xAudio2_) {
        instance.Initialize();
    }
    return &instance;
}

Audio::Audio() {}
Audio::~Audio() { Finalize(); }

void Audio::PauseAll() {
    if (xAudio2_ && !isPaused_) {
        xAudio2_->StopEngine();
        isPaused_ = true;
        Log::Write(L" ├─ 【音声一括ポーズ】 すべての音声再生を一時停止しました。");
    }
}

void Audio::ResumeAll() {
    if (xAudio2_ && isPaused_) {
        xAudio2_->StartEngine();
        isPaused_ = false;
        Log::Write(L" ├─ 【音声一括再開】 すべての音声再生を再開しました。");
    }
}

void Audio::Initialize() {
    // COM と Media Foundation の初期化
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);

    // XAudio2の初期化
    HRESULT hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

    hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(hr));

    Log::Write(L" ├─ 【音声システム初期化完了】 XAudio2 および Media Foundation の初期化に成功しました。");
}

void Audio::Finalize() {
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();

    MFShutdown();
    CoUninitialize();

    Log::Write(L" ├─ 【音声システム終了処理完了】 音声リソースのクリーンアップが完了しました。");
}

SoundData Audio::LoadSound(const std::string& filePath) {
    Log::Write(std::format(L" ├─ 【音声ロード開始】 ファイル:「{}」", ConvertString(filePath)));
    auto startTime = std::chrono::steady_clock::now();

    // UTF-8 → UTF-16変換
    std::wstring wFilePath = ConvertString(filePath);

    SoundData soundData{};
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(wFilePath.c_str(), nullptr, &reader);
    assert(SUCCEEDED(hr));

    // PCM形式に変換して読み込む
    Microsoft::WRL::ComPtr<IMFMediaType> audioType;
    MFCreateMediaType(&audioType);
    audioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, audioType.Get());

    // 出力フォーマット情報を取得
    Microsoft::WRL::ComPtr<IMFMediaType> outputType;
    reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputType);

    UINT32 blockAlign = 0;
    UINT32 bitsPerSample = 0;
    UINT32 samplesPerSec = 0;
    UINT32 channels = 0;
    outputType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);
    outputType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
    outputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSec);
    outputType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);

    // WAVEFORMATEX構築
    WAVEFORMATEX* wfex = new WAVEFORMATEX();
    wfex->wFormatTag = WAVE_FORMAT_PCM;
    wfex->nChannels = static_cast<WORD>(channels);
    wfex->nSamplesPerSec = samplesPerSec;
    wfex->wBitsPerSample = static_cast<WORD>(bitsPerSample);
    wfex->nBlockAlign = static_cast<WORD>(blockAlign);
    wfex->nAvgBytesPerSec = wfex->nSamplesPerSec * wfex->nBlockAlign;
    wfex->cbSize = 0;

    soundData.wfex = wfex;

    // 音声データを読み取り
    Microsoft::WRL::ComPtr<IMFSample> sample;
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    DWORD flags = 0;
    while (SUCCEEDED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample)) && sample) {
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        sample->ConvertToContiguousBuffer(&buffer);

        BYTE* audioPtr = nullptr;
        DWORD audioSize = 0;
        buffer->Lock(&audioPtr, nullptr, &audioSize);

        size_t start = soundData.audioData.size();
        soundData.audioData.resize(start + audioSize);
        memcpy(&soundData.audioData[start], audioPtr, audioSize);

        buffer->Unlock();
        buffer.Reset();
        sample.Reset();
    }

    auto endTime = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(endTime - startTime).count();

    // 0.1秒以上かかった場合は「低速ロード」マークをつける
    static constexpr float kSlowLoadThreshold = 0.1f;
    std::wstring slowLoadWarning = L"";
    if (elapsed >= kSlowLoadThreshold) {
        slowLoadWarning = L"[★低速ロード] ";
    }

    Log::Write(std::format(L" ├─ 【音声ロード完了】 {}:「{}」 | チャンネル数: {} | サンプリングレート: {}Hz | ビット数: {} | 所要時間: {:.4f}秒",
        slowLoadWarning, ConvertString(filePath), channels, samplesPerSec, bitsPerSample, elapsed));

    return soundData;
}

void Audio::PlaySoundW(SoundData& soundData, float volume, bool loop) {
    if (!xAudio2_ || !soundData.wfex || soundData.audioData.empty()) return;

    // 既にボイスが存在する場合は、古いボイスを安全に停止・破棄してから新規作成する（孤立リーク完全防止）
    if (soundData.sourceVoice) {
        soundData.sourceVoice->Stop(0);
        soundData.sourceVoice->FlushSourceBuffers();
        soundData.sourceVoice->DestroyVoice();
        soundData.sourceVoice = nullptr;
    }

    HRESULT hr = xAudio2_->CreateSourceVoice(&soundData.sourceVoice, soundData.wfex);
    assert(SUCCEEDED(hr));
    if (FAILED(hr) || !soundData.sourceVoice) return;

    hr = soundData.sourceVoice->SetVolume(volume);
    assert(SUCCEEDED(hr));

    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(soundData.audioData.size());
    buffer.pAudioData = soundData.audioData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    hr = soundData.sourceVoice->SubmitSourceBuffer(&buffer);
    assert(SUCCEEDED(hr));
    soundData.sourceVoice->Start(0);

    Log::Write(std::format(L" ├─ 【音声再生開始】 音声バッファをサブミットしました。ループ設定: {} | 音量: {:.2f}", loop, volume));
}

void Audio::StopSound(SoundData& soundData) {
    if (soundData.sourceVoice) {
        soundData.sourceVoice->Stop(0);
        soundData.sourceVoice->FlushSourceBuffers();
        soundData.sourceVoice->DestroyVoice();
        soundData.sourceVoice = nullptr;
        Log::Write(L" ├─ 【音声再生停止】 音声の再生を強制終了し、ボイスを破棄しました。");
    }
}

void Audio::Unload(SoundData& soundData) {
    if (soundData.wfex) {
        Log::Write(std::format(L" ├─ 【音声リソース解放開始】 チャンネル数: {} | サンプリングレート: {}Hz の音声データを解放します。",
            soundData.wfex->nChannels, soundData.wfex->nSamplesPerSec));
    }

    // 波形バッファを破棄する前に、必ずボイスを安全に停止・破棄する（アクセス違反クラッシュ防止）
    if (soundData.sourceVoice) {
        soundData.sourceVoice->Stop(0);
        soundData.sourceVoice->FlushSourceBuffers();
        soundData.sourceVoice->DestroyVoice();
        soundData.sourceVoice = nullptr;
    }

    if (soundData.wfex) {
        delete soundData.wfex;
        soundData.wfex = nullptr;
    }
    soundData.audioData.clear();
    Log::Write(L" └─ 【音声リソース解放完了】 音声バッファメモリおよび再生用ボイスの破棄が完了しました。");
}
