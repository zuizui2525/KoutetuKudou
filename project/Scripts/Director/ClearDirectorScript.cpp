#include "Scripts/Director/ClearDirectorScript.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectFactory.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Text/TextObject2D.h"
#include "Engine/Graphics/Text/TextTextureGenerator.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    const std::string kFireworkEffectName = "FireworksSet";
    constexpr float kRandCenterOffset = 0.5f;
    constexpr size_t kImGuiBufferLength = 128;
    const Vector3 kManualFireworkPos = { 0.0f, 15.0f, 20.0f }; // 手動発射位置 (上空)
    constexpr float kPi = 3.14159265f;
    constexpr int kLeftMouseButton = 0;
}

ClearDirectorScript::ClearDirectorScript()
    : ScriptComponent("ClearDirectorScript") {
}

ClearDirectorScript::~ClearDirectorScript() = default;

/// <summary>
/// ゲームクリアシーン開始時にラジアルブラーのポストプロセス演出を有効化し、
/// 祝砲花火エフェクトファクトリを初期化します。
/// </summary>
void ClearDirectorScript::Start() {
    // ポストプロセスの演出設定
    if (auto* postProcess = SceneManager::GetInstance()->GetPostProcess()) {
        postProcess->SetRadialBlurActive(true);
    }

    // エフェクトマネージャの初期化
    auto effectMgr = EffectManager::GetInstance();
    effectMgr->Initialize();
    EffectFactory::GetInstance()->RegisterAllEffects();
    fireworkTimer_ = 0;
    nextFireworkDelay_ = 10; // 開始直後に1発目をすぐ打ち上げ
    cameraOrbitAngle_ = 0.0f;
    victoryTimer_ = 0.0f;
    sceneFadeInAlpha_ = 1.0f; // 開始直後は100%真っ黒で準備前描画を完全隠蔽
}

/// <summary>
/// スペースキー入力によるタイトルシーンへの遷移監視、祝砲花火エフェクトの連続自動打ち上げ、
/// およびカメラの優雅なシネマティックオービット旋回を更新します。
/// </summary>
void ClearDirectorScript::OnUpdate() {
    // シーン開始時の明転（黒フェードイン）
    if (sceneFadeInAlpha_ > 0.0f) {
        sceneFadeInAlpha_ = (std::max)(0.0f, sceneFadeInAlpha_ - kFadeInSpeed);
    }

    // スペースキーまたは左クリックでタイトルシーンへ遷移
    auto input = InputResource::GetInput();
    if (input && (input->Trigger(DIK_SPACE) || input->MouseTrigger(kLeftMouseButton))) {
        SceneManager::GetInstance()->ChangeScene(nextSceneName_);
        return;
    }

    // 1. 祝砲GPU大玉花火（各3,000粒子）の連続自動打ち上げ
    fireworkTimer_++;
    if (fireworkTimer_ >= nextFireworkDelay_) {
        fireworkTimer_ = 0;
        nextFireworkDelay_ = minInterval_ + (rand() % (std::max)(1, maxIntervalRange_));

        float rx = ((static_cast<float>(rand()) / RAND_MAX) - kRandCenterOffset) * rangeX_;
        float ry = baseY_ + (static_cast<float>(rand()) / RAND_MAX) * rangeY_;
        float rz = baseZ_ + ((static_cast<float>(rand()) / RAND_MAX) - kRandCenterOffset) * rangeZ_;
        Vector3 spawnPos = { rx, ry, rz };

        EffectManager::GetInstance()->PlayGPUEffect(spawnPos, kGpuFireworkParticleCount);
    }

    // 2. カメラの優雅なオービット旋回（上空の花火を見上げるアングル）
    cameraOrbitAngle_ += kCameraOrbitSpeed;
    victoryTimer_ += 1.0f / 60.0f;

    if (auto* camMgr = CameraResource::GetCameraManager()) {
        if (auto* cam = camMgr->GetActiveCamera()) {
            float cx = std::sin(cameraOrbitAngle_) * kCameraOrbitRadius;
            float cz = std::cos(cameraOrbitAngle_) * kCameraOrbitRadius;
            float cy = kCameraOrbitHeight + std::sin(cameraOrbitAngle_ * 1.5f) * 1.0f;
            cam->SetPosition({ cx, cy, cz });

            // 上空（0, kCameraLookAtY, 15.0f）を見上げる視線
            Vector3 toTarget = { -cx, kCameraLookAtY - cy, 15.0f - cz };
            float distXZ = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
            float yaw = std::atan2(toTarget.x, toTarget.z);
            float pitch = -std::atan2(toTarget.y, distXZ > 0.001f ? distXZ : 0.001f);
            cam->SetRotation({ pitch, yaw, 0.0f });
        }
    }
}

/// <summary>
/// 祝砲花火エフェクトの3D描画を実行します（Scene::Draw()にて一括描画されるため空実装）。
/// </summary>
void ClearDirectorScript::Draw() {
}

/// <summary>
/// DirectX 12による豪華ビクトリーHUDの初期化および描画を実行します。
/// </summary>
void ClearDirectorScript::Draw2D() {
    if (!is2DInitialized_) {
        Init2DObjects();
    }
    RenderVictoryHUD();
}

void ClearDirectorScript::Init2DObjects() {
    if (is2DInitialized_) return;

    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);

    // シーン開始時の全画面黒フェードスプライト
    sceneFadeSprite_ = std::make_unique<SpriteObject>();
    sceneFadeSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(sceneFadeSprite_.get());
    sceneFadeSprite_->SetSize(kRenderWidth, kRenderHeight);
    sceneFadeSprite_->GetMaterialData()->color = { 0.0f, 0.0f, 0.0f, 1.0f };

    // ビクトリー中央バナー
    victoryBannerSprite_ = std::make_unique<SpriteObject>();
    victoryBannerSprite_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(victoryBannerSprite_.get());
    victoryBannerSprite_->SetSize(kBannerWidth, kBannerHeight);

    // タイトルテキスト
    victoryTitleTextObj_ = std::make_unique<TextObject2D>();
    victoryTitleTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(victoryTitleTextObj_.get());
    victoryTitleTextGen_ = std::make_unique<TextTextureGenerator>();

    // サブタイトルテキスト
    victorySubTextObj_ = std::make_unique<TextObject2D>();
    victorySubTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(victorySubTextObj_.get());
    victorySubTextGen_ = std::make_unique<TextTextureGenerator>();

    // PRESS SPACE 案内テキスト
    pressSpaceTextObj_ = std::make_unique<TextObject2D>();
    pressSpaceTextObj_->Initialize();
    SceneHierarchy::GetInstance()->Unregister(pressSpaceTextObj_.get());
    pressSpaceTextGen_ = std::make_unique<TextTextureGenerator>();

    is2DInitialized_ = true;
}

void ClearDirectorScript::RenderVictoryHUD() {
    constexpr float kRenderWidth = static_cast<float>(WindowApp::kClientWidth);
    constexpr float kRenderHeight = static_cast<float>(WindowApp::kClientHeight);

    // 1. 中央ビクトリーバナー（ダークサイバー調・半透明）
    float bannerX = (kRenderWidth - kBannerWidth) * 0.5f;
    float bannerY = (kRenderHeight * 0.32f) - (kBannerHeight * 0.5f);

    if (victoryBannerSprite_) {
        victoryBannerSprite_->SetPosition({ bannerX, bannerY, 0.0f });
        victoryBannerSprite_->SetSize(kBannerWidth, kBannerHeight);
        victoryBannerSprite_->GetMaterialData()->color = { 10.0f / 255.0f, 16.0f / 255.0f, 28.0f / 255.0f, 0.85f };
        victoryBannerSprite_->Update();
        victoryBannerSprite_->Draw("white", true);
    }

    // 2. メインタイトル「ALL STAGES CLEARED!!」（輝くゴールド）
    if (victoryTitleTextGen_ && victoryTitleTextObj_) {
        victoryTitleTextGen_->Generate("ALL STAGES CLEARED!!", "Impact", 48.0f, 2, true, false, false);
        float tw = static_cast<float>(victoryTitleTextGen_->GetWidth());
        float th = static_cast<float>(victoryTitleTextGen_->GetHeight());
        float tx = (kRenderWidth - tw) * 0.5f;
        float ty = bannerY + 24.0f;

        victoryTitleTextObj_->SetPosition({ tx, ty, 0.0f });
        victoryTitleTextObj_->SetSize(tw, th);
        victoryTitleTextObj_->SetColor({ 1.0f, 0.85f, 0.20f, 1.0f }); // 黄金
        victoryTitleTextObj_->Update();
        victoryTitleTextObj_->Draw(victoryTitleTextGen_->GetGpuHandle(), true);
    }

    // 3. サブタイトル「MISSION ACCOMPLISHED! ALL SECTORS LIBERATED!」（鮮やかなシアン）
    if (victorySubTextGen_ && victorySubTextObj_) {
        victorySubTextGen_->Generate("MISSION ACCOMPLISHED! ALL SECTORS LIBERATED!", "Yu Gothic UI", 18.0f, 1, true, false, false);
        float stw = static_cast<float>(victorySubTextGen_->GetWidth());
        float sth = static_cast<float>(victorySubTextGen_->GetHeight());
        float stx = (kRenderWidth - stw) * 0.5f;
        float sty = bannerY + 98.0f;

        victorySubTextObj_->SetPosition({ stx, sty, 0.0f });
        victorySubTextObj_->SetSize(stw, sth);
        victorySubTextObj_->SetColor({ 0.35f, 0.90f, 1.0f, 1.0f }); // ネオンシアン
        victorySubTextObj_->Update();
        victorySubTextObj_->Draw(victorySubTextGen_->GetGpuHandle(), true);
    }

    // 4. 「>> PRESS SPACE OR CLICK TO RETURN TO TITLE <<」（パルス点滅ホワイト）
    if (pressSpaceTextGen_ && pressSpaceTextObj_) {
        pressSpaceTextGen_->Generate(">> PRESS SPACE OR CLICK TO RETURN TO TITLE <<", "Yu Gothic UI", 20.0f, 1, true, false, false);
        float ptw = static_cast<float>(pressSpaceTextGen_->GetWidth());
        float pth = static_cast<float>(pressSpaceTextGen_->GetHeight());
        float ptx = (kRenderWidth - ptw) * 0.5f;
        float pty = bannerY + kBannerHeight - pth - 22.0f;

        float blink = 0.55f + 0.45f * std::sin(victoryTimer_ * kPressSpaceBlinkFreq);
        pressSpaceTextObj_->SetPosition({ ptx, pty, 0.0f });
        pressSpaceTextObj_->SetSize(ptw, pth);
        pressSpaceTextObj_->SetColor({ 1.0f, 1.0f, 1.0f, blink });
        pressSpaceTextObj_->Update();
        pressSpaceTextObj_->Draw(pressSpaceTextGen_->GetGpuHandle(), true);
    }

    // 5. シーン開始時の全画面黒フェードイン（準備前描画の完全隠蔽オーバーレイ）
    if (sceneFadeSprite_ && sceneFadeInAlpha_ > 0.005f) {
        sceneFadeSprite_->SetPosition({ 0.0f, 0.0f, 0.0f });
        sceneFadeSprite_->SetSize(kRenderWidth, kRenderHeight);
        sceneFadeSprite_->GetMaterialData()->color = { 0.0f, 0.0f, 0.0f, sceneFadeInAlpha_ };
        sceneFadeSprite_->Update();
        sceneFadeSprite_->Draw("white", true);
    }
}

void ClearDirectorScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char buffer[kImGuiBufferLength];
    strncpy_s(buffer, nextSceneName_.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputText("Next Scene", buffer, sizeof(buffer))) {
        nextSceneName_ = buffer;
    }
    ImGui::DragInt("Min Interval", &minInterval_, 1.0f, 1, 300);
    ImGui::DragInt("Interval Range", &maxIntervalRange_, 1.0f, 1, 300);
    ImGui::DragFloat("Range X (横幅)", &rangeX_, 1.0f, 0.0f, 200.0f);
    ImGui::DragFloat("Base Y (高さ基準)", &baseY_, 0.5f, -20.0f, 50.0f);
    ImGui::DragFloat("Range Y (高さ幅)", &rangeY_, 0.5f, 0.0f, 50.0f);
    ImGui::DragFloat("Base Z (奥行き基準)", &baseZ_, 0.5f, 0.0f, 100.0f);
    ImGui::DragFloat("Range Z (奥行き幅)", &rangeZ_, 0.5f, 0.0f, 100.0f);

    ImGui::Separator();
    if (ImGui::Button("🎆 GPU祝砲花火を手動発射 (3,000粒子)")) {
        EffectManager::GetInstance()->PlayGPUEffect(kManualFireworkPos, kGpuFireworkParticleCount);
    }
    ImGui::Checkbox("「エフェクト一覧」ウィンドウを表示", EffectManager::GetShowListWindowPtr());
#endif
}

REGISTER_SCRIPT(ClearDirectorScript);
