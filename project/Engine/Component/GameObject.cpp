#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/Components/AudioSourceComponent.h"
#include "Engine/Component/Components/ParticleEffectComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクターUI調整用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinScale = 0.001f;
    constexpr float kMaxScale = 1000.0f;
    constexpr float kButtonPadding = 5.0f;

    void SyncGPUForDeletion() {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
    }
}

GameObject::GameObject(const std::string& name) {
    name_ = name;
    Initialize();
}

GameObject::~GameObject() {
    SyncGPUForDeletion();
    components_.clear();
}

void GameObject::RemoveComponentByIndex(size_t index) {
    if (index < components_.size()) {
        SyncGPUForDeletion();
        components_.erase(components_.begin() + index);
    }
}

void GameObject::Initialize() {
    InitializeGameObject(name_);
    UpdateMatrix();
}

void GameObject::UpdateMatrix() {
    matWorld_ = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    isMatrixDirty_ = false;
}

void GameObject::Update() {
    if (!isVisible_) return;

    if (isMatrixDirty_) {
        UpdateMatrix();
    }

    // 全アクティブコンポーネントの更新
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            comp->Update();
        }
    }
}

void GameObject::Draw() {
    if (!isVisible_) return;

    // 全アクティブコンポーネントの3D描画
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            comp->Draw();
        }
    }
}

void GameObject::Draw2D() {
    if (!isVisible_) return;

    // 全アクティブコンポーネントの2D描画
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            comp->Draw2D();
        }
    }
}

void GameObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##" + name_;

    // Transform 操作ヘッダー
    if (ImGui::CollapsingHeader(("Transform" + idPrefix).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat3(("Position" + idPrefix).c_str(), &transform_.translate.x, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
            isMatrixDirty_ = true;
        }
        if (ImGui::DragFloat3(("Rotation" + idPrefix).c_str(), &transform_.rotate.x, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
            isMatrixDirty_ = true;
        }
        if (ImGui::DragFloat3(("Scale" + idPrefix).c_str(), &transform_.scale.x, kDragSpeed, kMinScale, kMaxScale, "%.2f")) {
            isMatrixDirty_ = true;
        }
    }

    if (isMatrixDirty_) {
        UpdateMatrix();
    }

    ImGui::Separator();

    // 保持している各コンポーネントの描画
    int removeIndex = -1;
    for (size_t i = 0; i < components_.size(); ++i) {
        auto& comp = components_[i];
        if (!comp) continue;

        ImGui::PushID(static_cast<int>(i));

        std::string headerName = comp->GetComponentTypeName();
        bool isComponentAlive = true;
        // ImGui公式のクローズボックス付きCollapsingHeaderを使用
        bool isHeaderOpen = ImGui::CollapsingHeader(headerName.c_str(), &isComponentAlive, ImGuiTreeNodeFlags_DefaultOpen);

        if (!isComponentAlive) {
            removeIndex = static_cast<int>(i);
        }

        if (isHeaderOpen && isComponentAlive) {
            // アクティブトグル
            bool active = comp->IsActive();
            if (ImGui::Checkbox("Enabled", &active)) {
                comp->SetActive(active);
            }

            // コンポーネント独自のインスペクターUI呼び出し
            comp->DrawInspector();
            ImGui::Spacing();
        }

        ImGui::PopID();
    }

    // 削除リクエストがあったコンポーネントを安全に削除
    if (removeIndex >= 0) {
        RemoveComponentByIndex(static_cast<size_t>(removeIndex));
    }

    ImGui::Separator();
    ImGui::Spacing();

    // [+ Add Component] ボタン (UnityライクなUI)
    constexpr float kAddBtnHeight = 26.0f;
    if (ImGui::Button("+ Add Component", ImVec2(-1.0f, kAddBtnHeight))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    // サブメニューが右側（矢印の方向）に展開できるよう、右側余白を確保した座標にポップアップを配置
    constexpr float kPopupWidthEstimate = 200.0f;
    constexpr float kSubmenuWidthEstimate = 240.0f;
    constexpr float kMargin = 10.0f;
    const float totalMenuWidth = kPopupWidthEstimate + kSubmenuWidthEstimate;

    ImVec2 buttonMin = ImGui::GetItemRectMin();
    ImVec2 buttonMax = ImGui::GetItemRectMax();
    float displayRight = ImGui::GetIO().DisplaySize.x;
    float popupX = buttonMin.x;

    if (popupX + totalMenuWidth > displayRight) {
        popupX = (std::max)(kMargin, displayRight - totalMenuWidth - kMargin);
    }

    ImGui::SetNextWindowPos(ImVec2(popupX, buttonMax.y));
    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("-- Select Component --");
        ImGui::Separator();

        // コンポーネント選択肢
        if (ImGui::BeginMenu("Audio")) {
            if (ImGui::MenuItem("AudioSource (サウンド再生)")) {
                AddComponent<AudioSourceComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Physics / Collision")) {
            if (ImGui::MenuItem("BoxCollider (箱型当たり判定)")) {
                AddComponent<BoxColliderComponent>();
            }
            if (ImGui::MenuItem("SphereCollider (球型当たり判定)")) {
                AddComponent<SphereColliderComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Effects")) {
            if (ImGui::MenuItem("ParticleEffect (エフェクト発生)")) {
                AddComponent<ParticleEffectComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rendering")) {
            if (ImGui::MenuItem("MeshRenderer (3D形状描画)")) {
                AddComponent<MeshRendererComponent>();
            }
            if (ImGui::MenuItem("SpriteRenderer (2Dスプライト描画)")) {
                AddComponent<SpriteRendererComponent>();
            }
            if (ImGui::MenuItem("Camera (カメラ)")) {
                AddComponent<CameraComponent>();
            }
            if (ImGui::MenuItem("Light (光源)")) {
                AddComponent<LightComponent>();
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
#endif
}
