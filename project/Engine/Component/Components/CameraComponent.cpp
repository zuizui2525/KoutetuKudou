#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "externals/imgui/imgui.h"
#include <format>
#include <cmath>

namespace {
    // 定数定義 (マジックナンバー排除)
    constexpr float kDefaultFov = 0.45f;
    constexpr float kDefaultNearZ = 0.1f;
    constexpr float kDefaultFarZ = 1000.0f;
    constexpr float kDefaultAspect = 16.0f / 9.0f;

    // FOV 角度変換定数
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    constexpr float kMinFovDeg = 1.0f;
    constexpr float kMaxFovDeg = 179.0f;

    // クリップ面制限定数
    constexpr float kMinNearZ = 0.001f;
    constexpr float kMaxNearZ = 100.0f;
    constexpr float kMinFarZ = 1.0f;
    constexpr float kMaxFarZ = 100000.0f;

    // UI 操作スピード
    constexpr float kDragSpeedClip = 0.5f;
    constexpr float kDragSpeedTarget = 0.1f;
}

CameraComponent::CameraComponent()
    : fov_(kDefaultFov),
      aspectRatio_(kDefaultAspect),
      nearZ_(kDefaultNearZ),
      farZ_(kDefaultFarZ),
      useTarget_(false),
      target_{ 0.0f, 0.0f, 0.0f },
      isMainCamera_(true),
      isRegistered_(false) {
    cameraInstance_ = std::make_shared<BaseCamera>();
}

CameraComponent::~CameraComponent() {
    UnregisterFromCameraManager();
}

void CameraComponent::Initialize() {
    if (cameraInstance_) {
        cameraInstance_->Initialize();
        if (owner_) {
            cameraInstance_->SetPosition(owner_->GetPosition());
            cameraInstance_->SetRotation(owner_->GetRotate());
        }
    }
    UpdateProjection();
    RegisterToCameraManager();
}

void CameraComponent::Update() {
    if (!cameraInstance_) return;

    if (owner_) {
        // オーナー GameObject の名前変更を検知して再登録
        if (isRegistered_ && registeredName_ != owner_->GetName()) {
            UnregisterFromCameraManager();
            RegisterToCameraManager();
        } else if (!isRegistered_) {
            RegisterToCameraManager();
        }

        // オーナーの Transform をカメラへ同期
        cameraInstance_->SetPosition(owner_->GetPosition());
        cameraInstance_->SetRotation(owner_->GetRotate());

        if (useTarget_) {
            cameraInstance_->SetTarget(target_);
        } else {
            cameraInstance_->DisableTarget();
        }
    }

    cameraInstance_->Update();
}

void CameraComponent::DrawInspector() {
#ifdef _USEIMGUI
    std::string tag = "##Camera_" + (owner_ ? owner_->GetName() : "Unnamed");

    if (ImGui::CollapsingHeader(("Camera" + tag).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        // メインカメラ切替
        bool prevIsMain = isMainCamera_;
        if (ImGui::Checkbox(("Main Camera" + tag).c_str(), &isMainCamera_)) {
            if (isMainCamera_ && !prevIsMain) {
                if (auto cameraMgr = CameraResource::GetCameraManager()) {
                    std::string camName = owner_ ? owner_->GetName() : "Camera";
                    cameraMgr->SetActiveCamera(camName);
                }
            }
        }

        // 現在アクティブかどうかの表示 (マジックナンバー排除の上品なシアン色バッジ)
        if (auto cameraMgr = CameraResource::GetCameraManager()) {
            std::string activeCamName = cameraMgr->GetActiveCameraName();
            std::string myName = owner_ ? owner_->GetName() : "";
            bool isCurrentActive = (!myName.empty() && activeCamName == myName);
            if (isCurrentActive) {
                ImGui::SameLine();
                constexpr ImVec4 kActiveCameraTextColor = { 0.2f, 0.9f, 1.0f, 1.0f };
                ImGui::TextColored(kActiveCameraTextColor, "[★ 現在レンダリング中]");
            } else {
                ImGui::SameLine();
                if (ImGui::Button(("このカメラに切り替え" + tag).c_str())) {
                    cameraMgr->SetActiveCamera(myName);
                    isMainCamera_ = true;
                }
            }
        }

        // 画角 (FOV: 度数法で表示・編集)
        float fovDegrees = fov_ * kRadToDeg;
        if (ImGui::SliderFloat(("Field of View (deg)" + tag).c_str(), &fovDegrees, kMinFovDeg, kMaxFovDeg, "%.1f deg")) {
            fov_ = fovDegrees * kDegToRad;
            UpdateProjection();
        }

        // クリップ面 (Near / Far)
        if (ImGui::DragFloat(("Near Clip" + tag).c_str(), &nearZ_, kDragSpeedClip, kMinNearZ, kMaxNearZ, "%.3f")) {
            nearZ_ = (std::max)(kMinNearZ, nearZ_);
            UpdateProjection();
        }

        if (ImGui::DragFloat(("Far Clip" + tag).c_str(), &farZ_, kDragSpeedClip, kMinFarZ, kMaxFarZ, "%.1f")) {
            farZ_ = (std::max)(nearZ_ + kMinNearZ, farZ_);
            UpdateProjection();
        }

        // 注視点 (Target) モード
        ImGui::Separator();
        ImGui::Checkbox(("Use LookAt Target" + tag).c_str(), &useTarget_);
        if (useTarget_) {
            ImGui::DragFloat3(("Target Position" + tag).c_str(), &target_.x, kDragSpeedTarget);
        }
    }
#endif
}

void CameraComponent::SetFov(float fov) {
    fov_ = fov;
    UpdateProjection();
}

void CameraComponent::SetNearZ(float nearZ) {
    nearZ_ = nearZ;
    UpdateProjection();
}

void CameraComponent::SetFarZ(float farZ) {
    farZ_ = farZ;
    UpdateProjection();
}

void CameraComponent::SetAspectRatio(float aspect) {
    aspectRatio_ = aspect;
    UpdateProjection();
}

void CameraComponent::SetMainCamera(bool isMain) {
    isMainCamera_ = isMain;
    if (isMain && owner_) {
        if (auto cameraMgr = CameraResource::GetCameraManager()) {
            cameraMgr->SetActiveCamera(owner_->GetName());
        }
    }
}

const Matrix4x4& CameraComponent::GetViewMatrix() const {
    if (cameraInstance_) {
        return cameraInstance_->GetViewMatrix();
    }
    static const Matrix4x4 identity = Math::MakeIdentity();
    return identity;
}

const Matrix4x4& CameraComponent::GetProjectionMatrix() const {
    if (cameraInstance_) {
        return cameraInstance_->GetProjectionMatrix();
    }
    static const Matrix4x4 identity = Math::MakeIdentity();
    return identity;
}

void CameraComponent::CreateRay(const Vector2& screenPos, float windowWidth, float windowHeight, Vector3& rayStart, Vector3& rayDir) const {
    if (cameraInstance_) {
        cameraInstance_->CreateRay(screenPos, windowWidth, windowHeight, rayStart, rayDir);
    }
}

void CameraComponent::UpdateProjection() {
    if (cameraInstance_) {
        cameraInstance_->UpdateProjection(aspectRatio_);
    }
}

void CameraComponent::RegisterToCameraManager() {
    if (!cameraInstance_) return;
    auto cameraMgr = CameraResource::GetCameraManager();
    if (!cameraMgr) return;

    std::string name = owner_ ? owner_->GetName() : "Camera";
    cameraMgr->AddCamera(name, cameraInstance_);
    registeredName_ = name;
    isRegistered_ = true;

    // 既に他の有効なゲームカメラ（Editor以外）がアクティブに存在し、本カメラが追加カメラ（Camera1等）の場合は勝手に奪わない
    std::string currentActive = cameraMgr->GetActiveCameraName();
    bool hasActiveGameCamera = (!currentActive.empty() && currentActive != "Editor");
    if (hasActiveGameCamera && name != "Camera" && name != "Main") {
        isMainCamera_ = false;
    }

    if (isMainCamera_) {
        cameraMgr->SetActiveCamera(name, false);
    }
}

void CameraComponent::UnregisterFromCameraManager() {
    if (!isRegistered_) return;
    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr && !registeredName_.empty()) {
        cameraMgr->RemoveCamera(registeredName_);
    }
    isRegistered_ = false;
    registeredName_ = "";
}

Vector3 CameraComponent::GetCalculatedRotation() const {
    if (cameraInstance_) {
        return cameraInstance_->GetCalculatedRotation();
    }
    return owner_ ? owner_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f };
}
