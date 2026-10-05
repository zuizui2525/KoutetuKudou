#pragma once
#include <vector>
#include <memory>
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Point/PointLight.h"
#include "Engine/Graphics/Objects/Light/Spot/SpotLight.h"

#include "Engine/Base/Log/Log.h"
#include <format>

class LightComponent;

class LightManager {
public:
    void Initialize();
    void Update();
    void Clear();

    // LightComponent の追加・解除窓口
    void RegisterLightComponent(LightComponent* comp);
    void UnregisterLightComponent(LightComponent* comp);

    // 従来のライトの追加窓口 (後方互換用)
    void AddDirectionalLight(DirectionalLightObject* light) { 
        directionalLights_.push_back(light); 
        auto& d = light->GetLightData();
        Log::Write(std::format(L" ├─ 【平行光源登録】 方向: ({:.2f}, {:.2f}, {:.2f}) | カラー: ({:.2f}, {:.2f}, {:.2f}) | 輝度: {:.2f}",
            d.direction.x, d.direction.y, d.direction.z, d.color.x, d.color.y, d.color.z, d.intensity));
    }
    void AddPointLight(PointLightObject* light) { 
        pointLights_.push_back(light); 
        auto& d = light->GetLightData();
        Log::Write(std::format(L" ├─ 【点光源登録】 座標: ({:.2f}, {:.2f}, {:.2f}) | カラー: ({:.2f}, {:.2f}, {:.2f}) | 輝度: {:.2f} | 半径: {:.2f}",
            d.position.x, d.position.y, d.position.z, d.color.x, d.color.y, d.color.z, d.intensity, d.radius));
    }
    void AddSpotLight(SpotLightObject* light) { 
        spotLights_.push_back(light); 
        auto& d = light->GetLightData();
        Log::Write(std::format(L" ├─ 【スポットライト登録】 座標: ({:.2f}, {:.2f}, {:.2f}) | 方向: ({:.2f}, {:.2f}, {:.2f}) | カラー: ({:.2f}, {:.2f}, {:.2f}) | 輝度: {:.2f} | 射程: {:.2f}",
            d.position.x, d.position.y, d.position.z, d.direction.x, d.direction.y, d.direction.z, d.color.x, d.color.y, d.color.z, d.intensity, d.distance));
    }

    // GPUアドレス取得（ModelObjectのDrawで使用）
    D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGroupAddress() const {
        return directionalLightResource_->GetGPUVirtualAddress();
    }
    D3D12_GPU_VIRTUAL_ADDRESS GetPointLightGroupAddress() const {
        return pointLightResource_->GetGPUVirtualAddress();
    }
    D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightGroupAddress() const {
        return spotLightResource_->GetGPUVirtualAddress();
    }

private:
    // 平行光源(Group)用
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLightGroup* directionalLightData_ = nullptr;
    std::vector<DirectionalLightObject*> directionalLights_;

    // 点光源用
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
    PointLightGroup* pointLightData_ = nullptr;
    std::vector<PointLightObject*> pointLights_;

    // スポットライト用
    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
    SpotLightGroup* spotLightData_ = nullptr;
    std::vector<SpotLightObject*> spotLights_;

    // 新コンポーネントシステム用
    std::vector<LightComponent*> lightComponents_;
};
