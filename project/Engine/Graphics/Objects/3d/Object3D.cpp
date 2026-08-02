#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Zuizui.h"
#include <stdexcept>
#include <typeinfo>

Object3D::~Object3D() {
    // GPUリソースを遅延解放キューに退避（GPUが使い終わるまで保持される）
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (wvpResource_) {
        deferredMgr->Enqueue(std::move(wvpResource_));
    }
    if (materialResource_) {
        deferredMgr->Enqueue(std::move(materialResource_));
    }
}

namespace {
    const std::string kClassPrefix = "class ";
    const std::string kStructPrefix = "struct ";
    const std::string kObjectSuffix = "Object";

    std::string GetDefaultNameFromType(const std::type_info& typeInfo) {
        std::string rawName = typeInfo.name();
        
        if (rawName.rfind(kClassPrefix, 0) == 0) {
            rawName = rawName.substr(kClassPrefix.length());
        } else if (rawName.rfind(kStructPrefix, 0) == 0) {
            rawName = rawName.substr(kStructPrefix.length());
        }
        
        if (rawName.length() > kObjectSuffix.length() && 
            rawName.compare(rawName.length() - kObjectSuffix.length(), kObjectSuffix.length(), kObjectSuffix) == 0) {
            rawName = rawName.substr(0, rawName.length() - kObjectSuffix.length());
        }
        
        return rawName;
    }
}

void Object3D::Initialize(int lightingMode) {
    // WVPリソース作成
    wvpResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(TransformationMatrix));
    if (!wvpResource_) throw std::runtime_error("Failed to create wvpResource_");
    HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    if (FAILED(hr) || !wvpData_) throw std::runtime_error("Failed to map wvpResource_");
    wvpData_->WVP = Math::MakeIdentity();
    wvpData_->world = Math::MakeIdentity();

    // Materialリソース作成
    materialResource_ = DxUtils::CreateBufferResource(sEngine->GetDevice(), sizeof(Material));
    if (!materialResource_) throw std::runtime_error("Failed to create materialResource_");
    hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    if (FAILED(hr) || !materialData_) throw std::runtime_error("Failed to map materialResource_");
    materialData_->color = { 1,1,1,1 };
    materialData_->enableLighting = lightingMode;
    materialData_->uvtransform = Math::MakeIdentity();
    materialData_->shininess = 30.0f;
    materialData_->environmentCoefficient = 0.0f;

    // Transform初期化
    transform_.scale = { 1,1,1 };
    transform_.rotate = { 0,0,0 };
    uvTransform_ = { {1,1,1}, {0,0,0}, {0,0,0} };

    // ヒエラルキー自動登録
    InitializeGameObject(GetDefaultNameFromType(typeid(*this)));
}
void Object3D::ImGuiSRTControl(const std::string& name) {
#ifdef _USEIMGUI
    std::string label = "##" + name;

    if (ImGui::CollapsingHeader(("Transform" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Scale" + label).c_str(), &transform_.scale.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat3(("Rotate" + label).c_str(), &transform_.rotate.x, 0.01f, 0.0f, 0.0f, "%.1f");
        ImGui::DragFloat3(("Translate" + label).c_str(), &transform_.translate.x, 0.01f, 0.0f, 0.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader(("Material" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4(("Color" + label).c_str(), &materialData_->color.x, ImGuiColorEditFlags_AlphaBar);
    }
#endif
}

void Object3D::ImGuiLightingControl(const std::string& name) {
#ifdef _USEIMGUI
    std::string label = "##" + name;
    if (ImGui::CollapsingHeader(("Lighting" + label).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::RadioButton(("None" + label).c_str(), (int*)&materialData_->enableLighting, 0); ImGui::SameLine();
        ImGui::RadioButton(("Lambert" + label).c_str(), (int*)&materialData_->enableLighting, 1); ImGui::SameLine();
        ImGui::RadioButton(("HalfLambert" + label).c_str(), (int*)&materialData_->enableLighting, 2);
        ImGui::DragFloat(("Env Coefficient" + label).c_str(), &materialData_->environmentCoefficient, 0.01f, 0.0f, 10.0f);
    }
#endif
}

void Object3D::DrawInspector() {
#ifdef _USEIMGUI
    ImGuiSRTControl(name_);
    ImGuiLightingControl(name_);
#endif
}

Vector3 Object3D::GetWorldPosition() const {
    return { matWorld_.m[3][0], matWorld_.m[3][1], matWorld_.m[3][2] };
}
