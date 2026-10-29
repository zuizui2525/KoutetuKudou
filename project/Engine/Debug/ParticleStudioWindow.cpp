#ifdef _USEIMGUI
#include "Engine/Debug/ParticleStudioWindow.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>

namespace {
    // 背景色プリセット定数 (マジックナンバー排除)
    constexpr Vector4 kBgColorDarkGray  = { 0.12f, 0.12f, 0.14f, 1.0f };
    constexpr Vector4 kBgColorPureBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr Vector4 kBgColorBlue      = { 0.1f, 0.2f, 0.4f, 1.0f };
    constexpr Vector4 kBgColorLightGray = { 0.4f, 0.4f, 0.45f, 1.0f };

    constexpr const char* kBgColorNames[] = {
        "ダークグレー (標準)",
        "漆黒 (ブラック)",
        "ディープブルー",
        "ライトグレー"
    };

    // カメラ回転リミット定数
    constexpr float kPitchMin = -1.45f;
    constexpr float kPitchMax =  1.45f;
    constexpr float kFovY = 45.0f * (3.14159265f / 180.0f);
    constexpr float kNearZ = 0.1f;
    constexpr float kFarZ = 500.0f;

    // スライダー感度定数
    constexpr float kDragSpeedNormal = 0.05f;
    constexpr float kDragSpeedFast   = 0.1f;
    constexpr float kDragSpeedSlow   = 0.01f;
}

ParticleStudioWindow::ParticleStudioWindow() {
    ResetCamera();
}

ParticleStudioWindow::~ParticleStudioWindow() = default;

void ParticleStudioWindow::ResetCamera() {
    cameraTarget_ = { 0.0f, 1.0f, 0.0f };
    cameraYaw_ = 0.0f;
    cameraPitch_ = 0.35f;
    cameraDistance_ = kDefaultCameraDistance;
}

bool ParticleStudioWindow::Initialize() {
    if (isInitialized_) return true;

    auto engine = EngineResource::GetEngine();
    if (!engine) return false;
    ID3D12Device* device = engine->GetDevice();
    auto dxCommon = engine->GetDxCommon();
    if (!device || !dxCommon) return false;

    // 1. プレビュー専用の独立した RTV ヒープを作成 (1スロット)
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));
    if (FAILED(hr)) return false;

    rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    // 2. グローバル SRV ヒープから専用スロットを取得
    ID3D12DescriptorHeap* srvHeap = dxCommon->GetSrvHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    srvCpuHandle_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, kStudioSrvHeapIndex);
    srvGpuHandle_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, kStudioSrvHeapIndex);

    // 3. レンダーテクスチャの生成
    previewRenderTexture_ = std::make_unique<RenderTexture>();
    previewRenderTexture_->Initialize(
        device,
        kDefaultPreviewWidth,
        kDefaultPreviewHeight,
        dxCommon->GetRtvFormat(),
        kBgColorDarkGray,
        rtvHandle_,
        srvCpuHandle_,
        srvGpuHandle_,
        L"ParticleStudioPreview"
    );

    // 4. プレビュー専用深度ステンシルリソース & DSV ヒープの生成 (D24_UNORM_S8_UINT)
    depthStencilResource_ = DxUtils::CreateDepthStencilTextureResource(device, kDefaultPreviewWidth, kDefaultPreviewHeight);
    dsvHeap_ = DxUtils::CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);

    // 5. プレビュー専用パーティクルシステムの生成とデフォルトプリセット適用
    previewParticle_ = std::make_unique<GPUParticleObject>();
    if (!previewParticle_->Initialize(GPUParticleObject::kDefaultMaxParticles)) {
        return false;
    }

    currentSetting_ = GPUParticlePresets::CreateFire();
    previewParticle_->ApplySetting(currentSetting_);

    isInitialized_ = true;
    return true;
}

void ParticleStudioWindow::UpdateCamera() {
    // 球面座標からカメラのワールド座標を計算
    float cosPitch = std::cos(cameraPitch_);
    float sinPitch = std::sin(cameraPitch_);
    float cosYaw   = std::cos(cameraYaw_);
    float sinYaw   = std::sin(cameraYaw_);

    cameraPos_.x = cameraTarget_.x + cameraDistance_ * cosPitch * sinYaw;
    cameraPos_.y = cameraTarget_.y + cameraDistance_ * sinPitch;
    cameraPos_.z = cameraTarget_.z + cameraDistance_ * cosPitch * cosYaw;

    // View 行列の作成
    Vector3 up = { 0.0f, 1.0f, 0.0f };
    viewMatrix_ = Math::MakeLookAtMatrix(cameraPos_, cameraTarget_, up);

    // Projection 行列の作成
    float aspect = (viewportSize_.y > 0.0f) ? (viewportSize_.x / viewportSize_.y) : (16.0f / 9.0f);
    projMatrix_ = Math::MakePerspectiveFovMatrix(kFovY, aspect, kNearZ, kFarZ);
}

void ParticleStudioWindow::Update(float deltaTime) {
    if (!isInitialized_ || !previewParticle_) return;

    UpdateCamera();
    previewParticle_->SetCustomCamera(viewMatrix_, projMatrix_, cameraPos_);

    // 再生状態およびタイムスケールに応じた更新
    if (isPlaying_ || requestStepFrame_) {
        float effectiveDelta = requestStepFrame_ ? (1.0f / 60.0f) : (deltaTime * timeScale_);
        previewParticle_->Update(effectiveDelta);
        requestStepFrame_ = false;

        // 単発（自動発生OFF）エフェクトの場合、自動ループ再生
        if (isPlaying_ && !currentSetting_.isAutoSpawning && isAutoRepeatBurst_ && currentSetting_.burstCount > 0) {
            uint32_t activeCount = previewParticle_->GetEstimatedActiveCount();
            if (activeCount == 0) {
                burstRepeatTimer_ += effectiveDelta;
                if (burstRepeatTimer_ >= kDefaultBurstRepeatInterval) {
                    burstRepeatTimer_ = 0.0f;
                    previewParticle_->EmitBurst(currentSetting_.burstCount);
                }
            } else {
                burstRepeatTimer_ = 0.0f;
            }
        }
    }
}

void ParticleStudioWindow::RenderPreview(ID3D12GraphicsCommandList* cmdList) {
    if (!isInitialized_ || !cmdList || !previewRenderTexture_ || !previewParticle_) return;

    // 背景色の選択
    Vector4 clearColor = kBgColorDarkGray;
    switch (selectedBgColorIndex_) {
        case 1: clearColor = kBgColorPureBlack; break;
        case 2: clearColor = kBgColorBlue;      break;
        case 3: clearColor = kBgColorLightGray; break;
        default: break;
    }

    // 1. レンダーテクスチャと深度バッファを描画ターゲットとして設定・クリア
    previewRenderTexture_->PreDraw(cmdList, dsvHandle_, true);

    // 2. プレビュー用ビューポート & シザー矩形の設定
    D3D12_VIEWPORT vp{};
    vp.Width = static_cast<float>(kDefaultPreviewWidth);
    vp.Height = static_cast<float>(kDefaultPreviewHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &vp);

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(kDefaultPreviewWidth);
    scissor.bottom = static_cast<LONG>(kDefaultPreviewHeight);
    cmdList->RSSetScissorRects(1, &scissor);

    // 3. プレビュー用パーティクルの Compute ディスパッチ & 描画
    previewParticle_->DispatchCompute(cmdList);
    previewParticle_->Draw(cmdList, currentSetting_.textureKey);

    // 4. レンダーテクスチャをシェーダーリソース状態へ遷移
    previewRenderTexture_->PostDraw(cmdList);
}

void ParticleStudioWindow::Draw(bool* open, ID3D12GraphicsCommandList* cmdList) {
    if (!open || !(*open)) return;

    if (!isInitialized_) {
        Initialize();
    }

    // プレビューの描画コマンドを実行
    RenderPreview(cmdList);

    // メインスタジオウィンドウ (DockSpace対応・メニューバー付き)
    ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Particle Studio (GPUパーティクルスタジオ)###ParticleStudio", open, ImGuiWindowFlags_MenuBar)) {
        DrawToolbar();

        // 左右2ペイン分割 (左: ビューポート + コントロールバー / 境界スプリッター / 右: インスペクター)
        float availableWidth = ImGui::GetContentRegionAvail().x;

        // スプリッターの幅制約をクランプ (最小ビューポート幅と最小インスペクター幅)
        float maxInspectorWidth = (std::max)(kMinInspectorWidth, availableWidth - kMinViewportWidth - kSplitterWidth);
        inspectorWidth_ = std::clamp(inspectorWidth_, kMinInspectorWidth, maxInspectorWidth);

        float leftWidth = (std::max)(kMinViewportWidth, availableWidth - inspectorWidth_ - kSplitterWidth);

        // 左ペイン (メインビューポート)
        ImGui::BeginChild("ViewportArea", ImVec2(leftWidth, 0), false);
        {
            DrawViewport();
            DrawControlBar();
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);

        // 境界スプリッター (マウスドラッグでインスペクター幅を自由伸縮)
        constexpr ImVec4 kSplitterColorNormal  = { 0.20f, 0.21f, 0.24f, 0.6f };
        constexpr ImVec4 kSplitterColorHovered = { 0.35f, 0.60f, 0.95f, 0.9f };
        constexpr ImVec4 kSplitterColorActive  = { 0.25f, 0.70f, 1.00f, 1.0f };

        ImGui::PushStyleColor(ImGuiCol_Button, kSplitterColorNormal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSplitterColorHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSplitterColorActive);
        ImGui::Button("##vsplitter", ImVec2(kSplitterWidth, -1.0f));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            ImGui::SetTooltip("ドラッグしてインスペクター幅を調整");
        }
        if (ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            float mouseDeltaX = ImGui::GetIO().MouseDelta.x;
            inspectorWidth_ -= mouseDeltaX;
            inspectorWidth_ = std::clamp(inspectorWidth_, kMinInspectorWidth, maxInspectorWidth);
        }

        ImGui::SameLine(0.0f, 0.0f);

        // 右ペイン (スタジオ・インスペクター)
        ImGui::BeginChild("InspectorArea", ImVec2(0, 0), true);
        {
            DrawStudioInspector();
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // ウィザードモーダルの描画
    DrawWizardModal();
}

void ParticleStudioWindow::DrawToolbar() {
    if (ImGui::BeginMenuBar()) {
        // [＋ 新規作成(ウィザード)]
        if (ImGui::MenuItem("＋ 新規作成ウィザード")) {
            showWizardModal_ = true;
        }

        ImGui::Separator();

        // プリセット選択
        if (ImGui::BeginMenu("プリセット読込")) {
            auto presetNames = GPUParticlePresets::GetPresetNames();
            for (const auto& name : presetNames) {
                if (ImGui::MenuItem(name.c_str())) {
                    ApplyPreset(name);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // カメラ視点リセット
        if (ImGui::MenuItem("カメラリセット")) {
            ResetCamera();
        }

        ImGui::EndMenuBar();
    }

    // 上部ツールバー行
    ImGui::Text("エフェクト名:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    char nameBuf[64]{};
    strncpy_s(nameBuf, currentSetting_.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("##EffectName", nameBuf, sizeof(nameBuf))) {
        currentSetting_.name = nameBuf;
        if (previewParticle_) {
            previewParticle_->ApplySetting(currentSetting_);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("JSON保存")) {
        std::string path = GPUParticlePresets::kParticleDirectory + "/" + currentSetting_.name + GPUParticlePresets::kFileExtension;
        GPUParticlePresets::SaveToJson(currentSetting_, path);
    }

    ImGui::SameLine();
    uint32_t activeEstimated = previewParticle_ ? previewParticle_->GetEstimatedActiveCount() : 0;
    uint32_t maxParticleCount = previewParticle_ ? previewParticle_->GetMaxParticles() : 0;
    ImGui::TextDisabled(" | 生存数(推定): %u / %u", activeEstimated, maxParticleCount);

    ImGui::Separator();
}

void ParticleStudioWindow::DrawViewport() {
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    // 下部コントロールバーの高さ分を確保
    constexpr float kControlBarHeight = 110.0f;
    float vpHeight = (std::max)(150.0f, contentSize.y - kControlBarHeight);
    float vpWidth = contentSize.x;

    viewportSize_ = { vpWidth, vpHeight };

    // プレビューテクスチャの描画
    ImTextureID texID = (ImTextureID)srvGpuHandle_.ptr;
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImGui::Image(texID, ImVec2(vpWidth, vpHeight));

    // ビューポート上のマウス入力判定（オービットカメラ操作）
    isViewportHovered_ = ImGui::IsItemHovered();

    if (isViewportHovered_) {
        ImGuiIO& io = ImGui::GetIO();

        // 左ドラッグ: 注視点オービット回転
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            cameraYaw_   += io.MouseDelta.x * kCameraRotateSpeed;
            cameraPitch_ += io.MouseDelta.y * kCameraRotateSpeed;
            cameraPitch_ = std::clamp(cameraPitch_, kPitchMin, kPitchMax);
        }

        // 右ドラッグまたは中ドラッグ: ターゲットのパン移動
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            float cosYaw = std::cos(cameraYaw_);
            float sinYaw = std::sin(cameraYaw_);
            Vector3 right = { cosYaw, 0.0f, -sinYaw };
            Vector3 up    = { 0.0f, 1.0f, 0.0f };

            cameraTarget_.x -= (right.x * io.MouseDelta.x - up.x * io.MouseDelta.y) * kCameraPanSpeed;
            cameraTarget_.y -= (right.y * io.MouseDelta.x - up.y * io.MouseDelta.y) * kCameraPanSpeed;
            cameraTarget_.z -= (right.z * io.MouseDelta.x - up.z * io.MouseDelta.y) * kCameraPanSpeed;
        }

        // マウスホイール: ズームイン/アウト
        if (std::abs(io.MouseWheel) > 0.0f) {
            cameraDistance_ -= io.MouseWheel * kCameraZoomSpeed;
            cameraDistance_ = std::clamp(cameraDistance_, kMinCameraDistance, kMaxCameraDistance);
        }
    }
}

void ParticleStudioWindow::DrawControlBar() {
    ImGui::Separator();

    // 再生・一時停止・コマ送り・最初から再生
    if (isPlaying_) {
        if (ImGui::Button("一時停止", ImVec2(80, 24))) {
            isPlaying_ = false;
        }
    } else {
        if (ImGui::Button("再生", ImVec2(80, 24))) {
            isPlaying_ = true;
            // バースト専用（自動発生OFF）の場合は即座に発射
            if (!currentSetting_.isAutoSpawning && currentSetting_.burstCount > 0) {
                if (previewParticle_) previewParticle_->EmitBurst(currentSetting_.burstCount);
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("1フレーム進む", ImVec2(100, 24))) {
        isPlaying_ = false;
        requestStepFrame_ = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("最初から再生", ImVec2(100, 24))) {
        if (previewParticle_) {
            previewParticle_->Reset();
            previewParticle_->ApplySetting(currentSetting_);
            if (currentSetting_.burstCount > 0) {
                previewParticle_->EmitBurst(currentSetting_.burstCount);
            }
        }
        isPlaying_ = true;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("再生速度", &timeScale_, 0.1f, 3.0f, "%.1fx");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("背景色", &selectedBgColorIndex_, kBgColorNames, IM_ARRAYSIZE(kBgColorNames));

    ImGui::SameLine();
    ImGui::Checkbox("自動ループ (Loop)", &isAutoRepeatBurst_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("単発（バースト）系エフェクトが消滅した際、一定間隔で自動再発射します");
    }

    // バースト発射テストボタン群
    ImGui::Text("バースト発射テスト:");
    ImGui::SameLine();
    if (ImGui::Button("Burst 100")) {
        if (previewParticle_) previewParticle_->EmitBurst(100);
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst 1,000")) {
        if (previewParticle_) previewParticle_->EmitBurst(1000);
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst 5,000")) {
        if (previewParticle_) previewParticle_->EmitBurst(5000);
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst 10,000")) {
        if (previewParticle_) previewParticle_->EmitBurst(10000);
    }
}

void ParticleStudioWindow::DrawStudioInspector() {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "スタジオ・インスペクター");
    ImGui::Separator();

    // ラベル表示幅を確実に確保し、文字見切れを根本防止
    ImGui::PushItemWidth(-kInspectorLabelReservedWidth);

    bool changed = false;

    // 1. エミッター設定
    if (ImGui::CollapsingHeader("1. エミッター (Emitter)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("自動発生 (Auto Spawning)", &currentSetting_.isAutoSpawning)) changed = true;
        if (ImGui::DragFloat("発生数/秒", &currentSetting_.spawnRatePerSec, 10.0f, 0.0f, 100000.0f, "%.0f 粒/秒")) changed = true;
        if (ImGui::DragFloat2("寿命範囲 (Min/Max)", &currentSetting_.minLifeTime, kDragSpeedSlow, 0.05f, 10.0f, "%.2f 秒")) changed = true;
        if (ImGui::DragFloat3("エミッター位置", &currentSetting_.emitterPosition.x, kDragSpeedNormal)) changed = true;
    }

    // 2. 物理・運動設定
    if (ImGui::CollapsingHeader("2. 物理・初速 (Physics & Motion)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat3("初速 (最小)", &currentSetting_.minVelocity.x, kDragSpeedNormal, -50.0f, 50.0f)) changed = true;
        if (ImGui::DragFloat3("初速 (最大)", &currentSetting_.maxVelocity.x, kDragSpeedNormal, -50.0f, 50.0f)) changed = true;
        if (ImGui::DragFloat("重力加速度", &currentSetting_.gravity, kDragSpeedNormal, -30.0f, 30.0f, "%.2f")) changed = true;
        if (ImGui::DragFloat("消滅高度 (Y)", &currentSetting_.killAtY, kDragSpeedNormal, -999.0f, 100.0f, "%.1f")) changed = true;
    }

    // 3. スケール・変形設定
    if (ImGui::CollapsingHeader("3. スケール変化 (Scale)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat3("開始スケール", &currentSetting_.startScale.x, kDragSpeedSlow, 0.01f, 10.0f)) changed = true;
        if (ImGui::DragFloat3("消滅時スケール", &currentSetting_.endScale.x, kDragSpeedSlow, 0.0f, 10.0f)) changed = true;
    }

    // 4. カラーグラデーション
    if (ImGui::CollapsingHeader("4. 色彩・透明度 (Color & Alpha)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::ColorEdit4("発生時カラー", &currentSetting_.startColor.x)) changed = true;
        if (ImGui::ColorEdit4("消滅時カラー", &currentSetting_.endColor.x)) changed = true;
    }

    // 5. テクスチャ設定
    if (ImGui::CollapsingHeader("5. テクスチャ (Texture & Material)", ImGuiTreeNodeFlags_DefaultOpen)) {
        char texBuf[64]{};
        strncpy_s(texBuf, currentSetting_.textureKey.c_str(), sizeof(texBuf) - 1);
        if (ImGui::InputText("テクスチャ名", texBuf, sizeof(texBuf))) {
            currentSetting_.textureKey = texBuf;
            changed = true;
        }
        ImGui::TextDisabled("※例: white, circle, star 等");
    }

    if (changed && previewParticle_) {
        previewParticle_->ApplySetting(currentSetting_);
    }

    ImGui::PopItemWidth();
}

void ParticleStudioWindow::DrawWizardModal() {
    if (showWizardModal_) {
        ImGui::OpenPopup("新規パーティクル作成ウィザード###WizardModal");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 420));

    if (ImGui::BeginPopupModal("新規パーティクル作成ウィザード###WizardModal", &showWizardModal_, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "作成したいエフェクトのタイプを選択してください:");
        ImGui::Separator();

        struct WizardOption {
            const char* label;
            const char* desc;
            const char* presetName;
        };

        const WizardOption options[] = {
            { "[炎・燃焼] Fire", "赤〜黄のグラデーションで上昇し、縮小しながら消滅します", "Fire" },
            { "[爆発・インパクト] Explosion", "全方位へ球状に高速拡散し、一気に炸裂するバースト演出です", "Explosion" },
            { "[煙・蒸気] Smoke", "ゆっくりと広がりながら上昇し、薄れてフェードアウトします", "Smoke" },
            { "[火花・スパーク] Sparks", "高初速で鋭く飛び散り、重力で地面に向かって落下します", "Sparks" },
            { "[吹雪・降雪] Snow", "上空から広範囲にゆっくりと舞い落ちる環境エフェクトです", "Snow" },
            { "[魔法オーラ] MagicAura", "シアンと紫の光がエミッター周辺から神秘的に立ち昇ります", "MagicAura" },
            { "[花火] Fireworks", "上空で炸裂し、多色の火花が華やかに拡散します", "Fireworks" }
        };

        for (const auto& opt : options) {
            if (ImGui::Button(opt.label, ImVec2(-1.0f, 32.0f))) {
                ApplyPreset(opt.presetName);
                showWizardModal_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextDisabled("   %s", opt.desc);
            ImGui::Spacing();
        }

        ImGui::Separator();
        if (ImGui::Button("キャンセル", ImVec2(-1.0f, 26.0f))) {
            showWizardModal_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ParticleStudioWindow::ApplyPreset(const std::string& presetName) {
    currentSetting_ = GPUParticlePresets::GetPresetByName(presetName);
    if (previewParticle_) {
        previewParticle_->Reset();
        previewParticle_->ApplySetting(currentSetting_);
        // 自動発生OFF（爆発や花火などバースト系）の場合は初回バーストを即座に発射
        if (!currentSetting_.isAutoSpawning && currentSetting_.burstCount > 0) {
            previewParticle_->EmitBurst(currentSetting_.burstCount);
        }
    }
    // カメラ注視点をエミッターの高さへ自動調整
    cameraTarget_ = currentSetting_.emitterPosition;
    isPlaying_ = true;
    burstRepeatTimer_ = 0.0f;
}

#endif
