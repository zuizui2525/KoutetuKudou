#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/Components/AudioSourceComponent.h"
#include "Engine/Component/Components/ParticleEffectComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/Components/TextRenderer3DComponent.h"
#include "Engine/Component/Components/RotatorComponent.h"
#include "Engine/Component/Components/SinOscillatorComponent.h"
#include "Engine/Component/Components/BlinkComponent.h"
#include "Engine/Component/Components/CameraOrbitComponent.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include <cmath>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクターUI調整用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinScale = 0.001f;
    constexpr float kMaxScale = 1000.0f;
    constexpr float kButtonPadding = 5.0f;

    // 2D初期配置用定数 (マジックナンバー排除)
    constexpr float kOriginDistanceThreshold = 10.0f;
    constexpr float kDefaultCenterRatio = 0.5f;

    // 3D適正座標判定用定数 (マジックナンバー排除)
    constexpr float kMax3DReasonableDistance = 50.0f; // これを超えるX/Y座標は2Dスクリーンピクセル座標の残留と判定

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

bool GameObject::Has2DRenderer() const {
    return HasComponent<SpriteRendererComponent>() ||
           HasComponent<TextRenderer2DComponent>();
}

bool GameObject::Has3DRenderer() const {
    return HasComponent<MeshRendererComponent>() ||
           HasComponent<TextRenderer3DComponent>();
}

bool GameObject::IsPosition2DScale() const {
    return std::abs(transform_.translate.x) > kMax3DReasonableDistance ||
           std::abs(transform_.translate.y) > kMax3DReasonableDistance;
}

void GameObject::Ensure2DPosition(bool force) {
    if (force || Math::Length(transform_.translate) < kOriginDistanceThreshold) {
        float centerX = static_cast<float>(WindowApp::kClientWidth) * kDefaultCenterRatio;
        float centerY = static_cast<float>(WindowApp::kClientHeight) * kDefaultCenterRatio;
        transform_.translate = { centerX, centerY, 0.0f };
        isMatrixDirty_ = true;
    }
}

void GameObject::Ensure3DPosition(bool force) {
    if (force) {
        transform_.translate = { 0.0f, 0.0f, 0.0f };
        isMatrixDirty_ = true;
    }
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

void GameObject::UpdateEditor() {
    if (!isVisible_) return;

    if (isMatrixDirty_) {
        UpdateMatrix();
    }

    // エディタ編集時も位置やテクスチャの同期が必要なレンダラー系のみ更新（アニメーション系はスキップ）
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            if (auto* tr2 = dynamic_cast<TextRenderer2DComponent*>(comp.get())) {
                tr2->Update();
            } else if (auto* tr3 = dynamic_cast<TextRenderer3DComponent*>(comp.get())) {
                tr3->Update();
            } else if (auto* mr = dynamic_cast<MeshRendererComponent*>(comp.get())) {
                mr->Update();
            } else if (auto* sr = dynamic_cast<SpriteRendererComponent*>(comp.get())) {
                sr->Update();
            }
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

        if (Is2D()) {
            ImGui::Spacing();
            if (ImGui::Button(("画面中央に配置 (2D)##Center_" + name_).c_str(), ImVec2(-1.0f, 0.0f))) {
                Ensure2DPosition(true);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("2D画面の中央 (X: 640, Y: 360) に配置します");
            }
        } else {
            // 3Dオブジェクト（またはレンダラー未設定時）
            if (IsPosition2DScale()) {
                ImGui::Spacing();
                static constexpr ImVec4 kWarningTextColor = { 1.0f, 0.8f, 0.2f, 1.0f }; // 黄色警告テキスト
                static constexpr ImVec4 kWarningButtonColor = { 0.8f, 0.45f, 0.1f, 1.0f }; // オレンジ強調ボタン
                ImGui::PushStyleColor(ImGuiCol_Text, kWarningTextColor);
                ImGui::TextWrapped("※ 座標が2D画面座標のままの可能性があります (X: %.1f, Y: %.1f)", transform_.translate.x, transform_.translate.y);
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Button, kWarningButtonColor);
                if (ImGui::Button(("3D原点 (0, 0, 0) にリセット##Reset3D_" + name_).c_str(), ImVec2(-1.0f, 0.0f))) {
                    Ensure3DPosition(true);
                }
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("3D空間の原点 (0, 0, 0) に座標をリセットし、カメラ視界内に戻します");
                }
            } else {
                ImGui::Spacing();
                if (ImGui::Button(("原点 (0, 0, 0) に配置##Origin3D_" + name_).c_str(), ImVec2(-1.0f, 0.0f))) {
                    Ensure3DPosition(true);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("座標を (0, 0, 0) にリセットします");
                }
            }
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
            const bool has2D = Has2DRenderer();
            const bool has3D = Has3DRenderer();

            // 3Dレンダラー（2Dレンダラー保持時は排他制御で無効化）
            if (ImGui::MenuItem("MeshRenderer (3D形状描画)", nullptr, false, !has2D)) {
                AddComponent<MeshRendererComponent>();
            }
            if (has2D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("2Dレンダラーを保持しているため、3Dレンダラーは追加できません");
            }

            if (ImGui::MenuItem("TextRenderer3D (3Dテキスト描画)", nullptr, false, !has2D)) {
                AddComponent<TextRenderer3DComponent>();
            }
            if (has2D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("2Dレンダラーを保持しているため、3Dレンダラーは追加できません");
            }

            // 2Dレンダラー（3Dレンダラー保持時は排他制御で無効化）
            if (ImGui::MenuItem("SpriteRenderer (2Dスプライト描画)", nullptr, false, !has3D)) {
                AddComponent<SpriteRendererComponent>();
                Ensure2DPosition();
            }
            if (has3D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("3Dレンダラーを保持しているため、2Dレンダラーは追加できません");
            }

            if (ImGui::MenuItem("TextRenderer2D (2Dテキスト描画)", nullptr, false, !has3D)) {
                AddComponent<TextRenderer2DComponent>();
                Ensure2DPosition();
            }
            if (has3D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("3Dレンダラーを保持しているため、2Dレンダラーは追加できません");
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Camera (カメラ)")) {
                AddComponent<CameraComponent>();
            }
            if (ImGui::MenuItem("Light (光源)")) {
                AddComponent<LightComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Animation / Motion")) {
            if (ImGui::MenuItem("Rotator (連続回転)")) {
                AddComponent<RotatorComponent>();
            }
            if (ImGui::MenuItem("SinOscillator (浮遊・往復運動)")) {
                AddComponent<SinOscillatorComponent>();
            }
            if (ImGui::MenuItem("Blink (点滅・明滅)")) {
                AddComponent<BlinkComponent>();
            }
            if (ImGui::MenuItem("CameraOrbit (カメラ自動旋回)")) {
                AddComponent<CameraOrbitComponent>();
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
#endif
}
