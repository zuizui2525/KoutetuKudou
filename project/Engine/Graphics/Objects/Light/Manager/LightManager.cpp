#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include <algorithm>

void LightManager::Initialize() {
    auto device = EngineResource::GetEngine()->GetDevice();

    // 平行光源グループのバッファ作成
    directionalLightResource_ = DxUtils::CreateBufferResource(device, sizeof(DirectionalLightGroup));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

    // 点光源グループのバッファ作成
    pointLightResource_ = DxUtils::CreateBufferResource(device, sizeof(PointLightGroup));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

    // スポットライトグループのバッファ作成
    spotLightResource_ = DxUtils::CreateBufferResource(device, sizeof(SpotLightGroup));
    spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

    Log::Write(L" ├─ 【ライト用バッファ初期化】 平行光源・点光源・スポットライト用バッファの作成・マッピングが完了しました。");
}

void LightManager::RegisterLightComponent(LightComponent* comp) {
    if (!comp) return;
    auto it = std::find(lightComponents_.begin(), lightComponents_.end(), comp);
    if (it == lightComponents_.end()) {
        lightComponents_.push_back(comp);
    }
}

void LightManager::UnregisterLightComponent(LightComponent* comp) {
    if (!comp) return;
    auto it = std::find(lightComponents_.begin(), lightComponents_.end(), comp);
    if (it != lightComponents_.end()) {
        lightComponents_.erase(it);
    }
}

void LightManager::Update() {
    // 平行光源のデータを集計
    int dirCount = 0;
    for (size_t i = 0; i < directionalLights_.size() && dirCount < kMaxDirectionalLights; ++i) {
        if (directionalLights_[i]) {
            directionalLightData_->lights[dirCount++] = directionalLights_[i]->GetLightData();
        }
    }
    for (auto* comp : lightComponents_) {
        if (dirCount >= kMaxDirectionalLights) break;
        if (comp && comp->IsActive() && comp->GetLightType() == LightComponent::LightType::Directional) {
            directionalLightData_->lights[dirCount++] = comp->GetDirectionalLightData();
        }
    }
    directionalLightData_->numLights = dirCount;

    // 点光源のデータを集計
    int pointCount = 0;
    for (size_t i = 0; i < pointLights_.size() && pointCount < kMaxPointLights; ++i) {
        if (pointLights_[i]) {
            pointLightData_->lights[pointCount++] = pointLights_[i]->GetLightData();
        }
    }
    for (auto* comp : lightComponents_) {
        if (pointCount >= kMaxPointLights) break;
        if (comp && comp->IsActive() && comp->GetLightType() == LightComponent::LightType::Point) {
            pointLightData_->lights[pointCount++] = comp->GetPointLightData();
        }
    }
    pointLightData_->numLights = pointCount;

    // スポットライトのデータを集計
    int spotCount = 0;
    for (size_t i = 0; i < spotLights_.size() && spotCount < kMaxSpotLights; ++i) {
        if (spotLights_[i]) {
            spotLightData_->lights[spotCount++] = spotLights_[i]->GetLightData();
        }
    }
    for (auto* comp : lightComponents_) {
        if (spotCount >= kMaxSpotLights) break;
        if (comp && comp->IsActive() && comp->GetLightType() == LightComponent::LightType::Spot) {
            spotLightData_->lights[spotCount++] = comp->GetSpotLightData();
        }
    }
    spotLightData_->numLights = spotCount;
}

void LightManager::Clear() {
    if (!directionalLights_.empty() || !pointLights_.empty() || !spotLights_.empty() || !lightComponents_.empty()) {
        Log::Write(L" ├─ 【ライトシステムクリア】 登録されていたすべての光源リソースを破棄しました。");
    }
    directionalLights_.clear();
    pointLights_.clear();
    spotLights_.clear();
    lightComponents_.clear();
}
