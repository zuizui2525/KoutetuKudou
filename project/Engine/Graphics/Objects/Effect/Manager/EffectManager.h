#pragma once
#include "../Settings/EffectSetting.h"
#include "../Core/BaseParticleObject.h"
#include <unordered_map>
#include <memory>
#include <string>

class EffectManager {
public:
    static EffectManager* GetInstance();

    void Initialize();
    void Finalize();
    void Update();
    void UpdateMatrices(); // ポーズ中の行列再計算用
    void Draw();

    void ImGuiControl(const std::string& name);

    // --- ウィンドウ表示制御 ---
    static bool* GetShowListWindowPtr() { return &showListWindow_; }
    static bool IsListWindowOpen() { return showListWindow_; }
    static void SetListWindowOpen(bool open) { showListWindow_ = open; }

    // エフェクト設定の登録
    void RegisterEffect(const EffectSetting& setting);

    // --- 新しい再生API ---
    
    // 2Dエフェクト再生
    void PlayEffect2D(const std::string& name, const EffectPlayParam& param);

    // 3Dエフェクト再生
    void PlayEffect3D(const std::string& name, const EffectPlayParam& param);

    // 登録済みのエフェクトオブジェクトを取得する
    BaseParticleObject* GetEffect(const std::string& name);

    // エミッターの停止
    void StopEffect(const std::string& name);



private:
    EffectManager() = default;
    ~EffectManager() = default;
    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

    std::unordered_map<std::string, std::unique_ptr<BaseParticleObject>> effectMap_;
    bool isWindowOpen_ = false;
    static inline bool showListWindow_ = true;
};
