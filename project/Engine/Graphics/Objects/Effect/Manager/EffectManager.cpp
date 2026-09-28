#include "EffectManager.h"
#include "../Core/SpriteParticleObject.h"
#include "../Core/MeshParticleObject.h"
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
    Log::Write(L" ├─ 【エフェクトシステム初期化】 エフェクトマネージャの初期化を完了しました。");
}

void EffectManager::Finalize() {
    Log::Write(L" ├─ 【エフェクトシステム終了処理開始】 登録されているすべてのエフェクトを解放します。");
    for (auto& pair : effectMap_) {
        Log::Write(std::format(L" │   ├─ 【エフェクト解放完了】 名前:「{}」をメモリから解放しました。", ConvertString(pair.first)));
    }
    effectMap_.clear();
    Log::Write(L" └─ 【エフェクトシステム終了処理完了】 すべてのエフェクトリソースの破棄が完了しました。");
}

void EffectManager::Update() {
    for (auto& pair : effectMap_) {
        pair.second->Update();
    }
}

void EffectManager::UpdateMatrices() {
    for (auto& pair : effectMap_) {
        pair.second->UpdateMatrices();
    }
}

void EffectManager::Draw() {
    for (auto& pair : effectMap_) {
        const std::string& texName = pair.second->GetSetting().textureName;
        pair.second->Draw(texName, true);
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
    auto it = effectMap_.find(name);
    if (it != effectMap_.end()) {
        it->second->SetEmitterMode(param.isLoop);

        if (!param.isLoop) {
            // 単発：エミッター自体は(0,0,0)のままで、指定座標に直接パーティクルを出す
            // (これでエミッター行列による二重適用を防ぐ)
            it->second->SetPosition({ 0.0f, 0.0f, 0.0f });

            const EffectSetting& setting = it->second->GetSetting();
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> distCount(setting.emitCountMin, setting.emitCountMax);
            uint32_t count = distCount(gen);

            if (count > 0) {
                it->second->EmitAt(count, param);
            }
        } else {
            // ループ：エミッター自体を移動・回転・スケールさせ、そこから出す
            Transform& transform = it->second->GetTransform();
            transform.translate = param.position;
            transform.rotate = param.rotation;
            transform.scale = param.scale;
        }
    }
}

void EffectManager::PlayEffect3D(const std::string& name, const EffectPlayParam& param) {
    auto it = effectMap_.find(name);
    if (it != effectMap_.end()) {
        it->second->SetEmitterMode(param.isLoop);

        if (!param.isLoop) {
            // 単発：エミッター自体は(0,0,0)のままで、指定座標に直接パーティクルを出す
            it->second->SetPosition({ 0.0f, 0.0f, 0.0f });

            const EffectSetting& setting = it->second->GetSetting();
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> distCount(setting.emitCountMin, setting.emitCountMax);
            uint32_t count = distCount(gen);

            if (count > 0) {
                it->second->EmitAt(count, param);
            }
        } else {
            // ループ：エミッター自体を移動・回転・スケールさせ、そこから出す
            Transform& transform = it->second->GetTransform();
            transform.translate = param.position;
            transform.rotate = param.rotation;
            transform.scale = param.scale;
        }
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
