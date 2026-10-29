#include "EffectManager.h"
#include "../Core/SpriteParticleObject.h"
#include "../Core/MeshParticleObject.h"
#include "../GPU/GPUParticleObject.h"
#include "../GPU/GPUParticleSetting.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <random>
#include <imgui.h>
#include <format>

EffectManager* EffectManager::GetInstance() {
    static EffectManager instance;
    return &instance;
}

void EffectManager::Initialize() {
    effectMap_.clear();
    gpuParticle_ = std::make_unique<GPUParticleObject>();
    gpuParticle_->Initialize(GPUParticleObject::kDefaultMaxParticles);
    gpuParticle_->Reset(); // 初期状態では粒子を一切放出せず待機
    Log::Write(L" ├─ 【エフェクトシステム初期化】 エフェクトマネージャおよびGPUパーティクルの初期化を完了しました。");
}

void EffectManager::Finalize() {
    Log::Write(L" ├─ 【エフェクトシステム終了処理開始】 登録されているすべてのエフェクトを解放します。");
    for (auto& pair : effectMap_) {
        Log::Write(std::format(L" │   ├─ 【エフェクト解放完了】 名前:「{}」をメモリから解放しました。", ConvertString(pair.first)));
    }
    effectMap_.clear();
    gpuParticle_.reset();
    Log::Write(L" └─ 【エフェクトシステム終了処理完了】 すべてのエフェクトリソースの破棄が完了しました。");
}

void EffectManager::Update() {
    for (auto& pair : effectMap_) {
        if (pair.second) {
            pair.second->Update();
        }
    }
    if (gpuParticle_) {
        gpuParticle_->Update();
    }
}

void EffectManager::UpdateMatrices() {
    for (auto& pair : effectMap_) {
        if (pair.second) {
            pair.second->UpdateMatrices();
        }
    }
}

void EffectManager::Draw() {
    for (auto& pair : effectMap_) {
        if (pair.second) {
            pair.second->Draw();
        }
    }
    if (gpuParticle_) {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                auto* cmdList = dxCommon->GetCommandList();
                gpuParticle_->DispatchCompute(cmdList);
                gpuParticle_->Draw(cmdList, "white", gpuParticle_->GetBlendMode());
            }
        }
    }
}

void EffectManager::PlayGPUEffect(const Vector3& position, uint32_t count) {
    if (gpuParticle_) {
        gpuParticle_->SetPosition(position);
        gpuParticle_->EmitBurst(count);
    }
}

void EffectManager::RegisterEffect(const EffectSetting& setting) {
    std::unique_ptr<BaseParticleObject> particle;

    if (setting.meshType == "cube" || setting.meshType == "flat_ring" || setting.meshType == "cylinder") {
        particle = std::make_unique<MeshParticleObject>();
    } else {
        particle = std::make_unique<SpriteParticleObject>();
    }
    
    particle->SetSetting(setting);
    particle->Initialize();
    particle->SetEmitterMode(false); // 初期状態はOFF
    
    effectMap_[setting.name] = std::move(particle);

    Log::Write(std::format(L" ├─ 【エフェクト登録完了】 名前:「{}」 | メッシュタイプ:「{}」 | テクスチャ:「{}」", 
        ConvertString(setting.name), ConvertString(setting.meshType), ConvertString(setting.textureName)));
}

void EffectManager::PlayEffect2D(const std::string& name, const EffectPlayParam& param) {
    if (gpuParticle_) {
        GPUParticleSetting setting;
        std::string jsonPath = GPUParticlePresets::kParticleDirectory + "/" + name + GPUParticlePresets::kFileExtension;
        if (!GPUParticlePresets::LoadFromJson(jsonPath, setting)) {
            setting = GPUParticlePresets::GetPresetByName(name);
        }
        gpuParticle_->ApplySetting(setting);
        gpuParticle_->SetPosition(param.position);
        constexpr uint32_t kDefaultGpuBurstCount2D = 300;
        uint32_t burst = (setting.burstCount > 0) ? setting.burstCount : kDefaultGpuBurstCount2D;
        gpuParticle_->EmitBurst(burst);
    }
}

void EffectManager::PlayEffect3D(const std::string& name, const EffectPlayParam& param) {
    // 1. effectMap_ に登録されたエフェクト（BulletTrail等）があれば各弾丸の個別位置で独立放出
    auto it = effectMap_.find(name);
    if (it != effectMap_.end() && it->second) {
        constexpr uint32_t kCpuEmitCountPerPlay = 1;
        it->second->EmitAt(kCpuEmitCountPerPlay, param);
        return;
    }

    // 2. なければGPUパーティクルで再生
    if (gpuParticle_) {
        GPUParticleSetting setting;
        std::string jsonPath = GPUParticlePresets::kParticleDirectory + "/" + name + GPUParticlePresets::kFileExtension;
        if (!GPUParticlePresets::LoadFromJson(jsonPath, setting)) {
            setting = GPUParticlePresets::GetPresetByName(name);
        }

        // ループ指定がないワンショット再生の場合は、自動連続発生を無効化してバーストのみ実行
        if (!param.isLoop) {
            setting.isAutoSpawning = false;
            setting.spawnRatePerSec = 0.0f;
        }

        gpuParticle_->ApplySetting(setting);
        gpuParticle_->SetPosition(param.position);
        constexpr uint32_t kDefaultGpuBurstCount3D = 30; // 控えめでシンプルな発生数
        constexpr uint32_t kMaxGpuBurstCount3D = 50;
        uint32_t burst = (setting.burstCount > 0) ? (std::min)(setting.burstCount, kMaxGpuBurstCount3D) : kDefaultGpuBurstCount3D;
        gpuParticle_->EmitBurst(burst);
    }
}

BaseParticleObject* EffectManager::GetEffect(const std::string& name) {
    auto it = effectMap_.find(name);
    if (it != effectMap_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void EffectManager::StopEffect(const std::string& name) {
    auto it = effectMap_.find(name);
    if (it != effectMap_.end()) {
        it->second->SetEmitterMode(false);
    }
}

void EffectManager::ImGuiControl(const std::string& name) {
#ifdef _USEIMGUI
    (void)name;

    // 1. エフェクト一覧ウィンドウ
    if (showListWindow_) {
        if (ImGui::Begin("エフェクト一覧###Effects List", &showListWindow_)) {
            ImGui::Checkbox("エフェクト詳細設定", &isWindowOpen_);
            ImGui::Separator();
            if (gpuParticle_) {
                gpuParticle_->ImGuiControl("GPUパーティクル (Compute Shader)");
            }
        }
        ImGui::End();
    }

    // 2. エフェクト詳細設定ウィンドウ（個別コントロール）
    if (isWindowOpen_) {
        if (ImGui::Begin("エフェクト詳細設定###Effects Control", &isWindowOpen_)) {
            // スライダー調整用定数（マジックナンバー排除）
            constexpr float kFloatDragSpeed = 0.1f;
            constexpr float kScaleDragSpeed = 0.05f;
            constexpr float kLifeTimeMinLimit = 0.1f;
            constexpr float kLifeTimeMaxLimit = 10.0f;
            constexpr float kFreqMinLimit = 0.1f;
            constexpr float kFreqMaxLimit = 10.0f;
            constexpr int kEmitCountMinLimit = 1;
            constexpr int kEmitCountMaxLimit = 100;

            for (auto& pair : effectMap_) {
                std::string effectName = pair.first;
                EffectSetting& setting = pair.second->GetSettingRef();

                if (ImGui::CollapsingHeader(effectName.c_str())) {
                    std::string label = "##" + effectName;
                    
                    ImGui::Text("基本設定");
                    int emitMin = setting.emitCountMin;
                    int emitMax = setting.emitCountMax;
                    if (ImGui::DragInt(("最小放出数" + label).c_str(), &emitMin, 1, kEmitCountMinLimit, kEmitCountMaxLimit)) setting.emitCountMin = emitMin;
                    if (ImGui::DragInt(("最大放出数" + label).c_str(), &emitMax, 1, kEmitCountMinLimit, kEmitCountMaxLimit)) setting.emitCountMax = emitMax;
                    ImGui::DragFloat(("最小寿命 (秒)" + label).c_str(), &setting.lifeTimeMin, kFloatDragSpeed, kLifeTimeMinLimit, kLifeTimeMaxLimit);
                    ImGui::DragFloat(("最大寿命 (秒)" + label).c_str(), &setting.lifeTimeMax, kFloatDragSpeed, kLifeTimeMinLimit, kLifeTimeMaxLimit);

                    ImGui::SeparatorText("トランスフォーム (座標・サイズ・回転)");
                    ImGui::DragFloat3(("位置オフセット" + label).c_str(), &setting.positionOffset.x, kFloatDragSpeed);
                    ImGui::DragFloat3(("生成範囲 (最小)" + label).c_str(), &setting.spawnAreaMin.x, kFloatDragSpeed);
                    ImGui::DragFloat3(("生成範囲 (最大)" + label).c_str(), &setting.spawnAreaMax.x, kFloatDragSpeed);
                    ImGui::DragFloat3(("最小初速" + label).c_str(), &setting.velocityMin.x, kFloatDragSpeed);
                    ImGui::DragFloat3(("最大初速" + label).c_str(), &setting.velocityMax.x, kFloatDragSpeed);
                    ImGui::DragFloat3(("開始スケール (最小)" + label).c_str(), &setting.scaleMin.x, kScaleDragSpeed);
                    ImGui::DragFloat3(("開始スケール (最大)" + label).c_str(), &setting.scaleMax.x, kScaleDragSpeed);
                    ImGui::DragFloat3(("終了スケール (最小)" + label).c_str(), &setting.scaleEndMin.x, kScaleDragSpeed);
                    ImGui::DragFloat3(("終了スケール (最大)" + label).c_str(), &setting.scaleEndMax.x, kScaleDragSpeed);
                    ImGui::DragFloat3(("最小回転角" + label).c_str(), &setting.rotationMin.x, kFloatDragSpeed);
                    ImGui::DragFloat3(("最大回転角" + label).c_str(), &setting.rotationMax.x, kFloatDragSpeed);

                    ImGui::SeparatorText("特殊機能設定");
                    ImGui::Checkbox(("ビルボード (常にカメラを向く)" + label).c_str(), &setting.isBillboard);
                    ImGui::Checkbox(("エミッターモード (継続放出)" + label).c_str(), &setting.isEmitter);
                    if (setting.isEmitter) {
                        ImGui::DragFloat(("放出周期 (秒)" + label).c_str(), &setting.emitFrequency, kFloatDragSpeed, kFreqMinLimit, kFreqMaxLimit);
                    }

                    ImGui::SeparatorText("カラーグラデーション");
                    ImGui::ColorEdit4(("開始色 (最小)" + label).c_str(), &setting.colorStartMin.x);
                    ImGui::ColorEdit4(("開始色 (最大)" + label).c_str(), &setting.colorStartMax.x);
                    ImGui::ColorEdit4(("終了色 (最小)" + label).c_str(), &setting.colorEndMin.x);
                    ImGui::ColorEdit4(("終了色 (最大)" + label).c_str(), &setting.colorEndMax.x);
                }
            }
            ImGui::End();
        }
    }
#endif
}
