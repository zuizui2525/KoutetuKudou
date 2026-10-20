#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Debug/GameViewWindow.h"
#include <cmath>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(PlayerControllerScript)

namespace {
    constexpr float kMuzzleOffsetZ = 0.8f;
    constexpr float kRayTargetDistance = 50.0f;
    constexpr int kReloadKey = DIK_R;
}

PlayerControllerScript::PlayerControllerScript()
    : ScriptComponent("PlayerControllerScript") {
}

void PlayerControllerScript::Awake() {
    hp_ = maxHp_;
    ammo_ = maxAmmo_;
    reloadTimer_ = 0;
    shotIntervalTimer_ = 0;
    isDead_ = false;
    isAutoMoving_ = false;
}

void PlayerControllerScript::Start() {
}

void PlayerControllerScript::OnUpdate() {
    if (isDead_ || !owner_) {
        return;
    }

    // 連射間隔タイマーの更新
    if (shotIntervalTimer_ > 0) {
        shotIntervalTimer_--;
    }

    // リロードタイマーの更新
    if (reloadTimer_ > 0) {
        reloadTimer_--;
        if (reloadTimer_ == 0) {
            ammo_ = maxAmmo_;
        }
    }

    // 入力処理（リロード & 射撃: 走行中のみ実行可能とし、描画中の誤射を防止）
    if (isAutoMoving_) {
        HandleReload();
        HandleShooting();
    }
}

/// <summary>
/// リロードキー（Rキー）の押下を検知し、リロードタイマーを開始します。
/// </summary>
void PlayerControllerScript::HandleReload() {
    auto* input = InputResource::GetInput();
    if (!input) return;

    if (reloadTimer_ <= 0 && ammo_ < maxAmmo_) {
        if (input->Trigger(kReloadKey)) {
            reloadTimer_ = reloadDuration_;
        }
    }
}

/// <summary>
/// マウスの左クリック（単発）または長押し（連射）を監視し、
/// 連射インターバルと残弾数（または無限弾フラグ）の条件を満たしている場合に射撃を実行します。
/// </summary>
void PlayerControllerScript::HandleShooting() {
    auto* input = InputResource::GetInput();
    if (!input) return;

    bool canShoot = shotIntervalTimer_ <= 0 && (infiniteAmmo_ || (reloadTimer_ <= 0 && ammo_ > 0));
    if (canShoot) {
        // 左クリック（単発入力または長押し連射）
        if (input->MouseTrigger(kShotMouseKey) || input->MousePress(kShotMouseKey)) {
            Fire();
        }
    }
}

/// <summary>
/// 自機の銃口位置からマウス照準方向（カメラレイ交点）へ向けて弾丸を射出し、
/// 射撃反動によるカメラシェイクを誘発します。
/// 
/// 【照準計算アルゴリズム】
/// 1. ゲームビューのウィンドウサイズと解像度からマウス座標をクライアント空間へ補正。
/// 2. アクティブカメラからマウス座標を通る3D視線レイ（rayStart, rayDir）を生成。
/// 3. レイの前方（kRayTargetDistance）を着弾目標点（targetPos）として設定。
/// 4. 自機前方の銃口座標（muzzlePos）から目標点へのベクトルを正規化し、弾丸の初速ベクトルを決定。
/// 5. 弾丸プレハブ（Bullet）を実体化し、パラメータを設定。
/// </summary>
void PlayerControllerScript::Fire() {
    if (!infiniteAmmo_) {
        ammo_--;
    }
    shotIntervalTimer_ = shotInterval_;

    Vector3 playerPos = owner_->GetPosition();
    Vector3 muzzlePos = {
        playerPos.x + direction_.x * kMuzzleOffsetZ,
        playerPos.y + direction_.y * kMuzzleOffsetZ,
        playerPos.z + direction_.z * kMuzzleOffsetZ
    };

    // カメラとマウスカーソルによる照準レイの計算
    Vector3 bulletDir = direction_;
    auto* cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr) {
        BaseCamera* camera = cameraMgr->GetActiveCamera();
        if (camera) {
            float clientW = static_cast<float>(WindowApp::kClientWidth);
            float clientH = static_cast<float>(WindowApp::kClientHeight);
            Vector2 viewSize = GameViewWindow::GetGameViewSize();
            Vector2 mousePos = GameViewWindow::GetMousePosition();

            Vector2 scaledMousePos = mousePos;
            if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
                scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
                scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
            }

            Vector3 rayStart = camera->GetPosition();
            Vector3 rayDir = direction_;
            camera->CreateRay(scaledMousePos, clientW, clientH, rayStart, rayDir);

            Vector3 targetPos = {
                rayStart.x + rayDir.x * kRayTargetDistance,
                rayStart.y + rayDir.y * kRayTargetDistance,
                rayStart.z + rayDir.z * kRayTargetDistance
            };

            Vector3 diff = {
                targetPos.x - muzzlePos.x,
                targetPos.y - muzzlePos.y,
                targetPos.z - muzzlePos.z
            };
            float lenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            if (lenSq > 0.0001f) {
                float invLen = 1.0f / std::sqrt(lenSq);
                bulletDir = { diff.x * invLen, diff.y * invLen, diff.z * invLen };
            }
        }
    }

    // プレハブからBulletを実体化
    GameObject* bulletObj = ScriptComponent::Instantiate("Bullet", muzzlePos);
    if (bulletObj) {
        auto* bulletScript = bulletObj->GetComponent<BulletScript>();
        if (bulletScript) {
            bulletScript->SetDirection(bulletDir);
            bulletScript->SetSpeed(bulletSpeed_);
            bulletScript->SetIsPlayerBullet(true);
        }
    }

    // 射撃反動のカメラシェイク演出
    ShakeCamera(kShootShakeIntensity, kShootShakeDuration);
}

/// <summary>
/// 敵弾等からの被弾によるダメージをHPに減算し、HPが0に達した場合は撃墜（死亡）フラグを立てます。
/// </summary>
/// <param name="amount">ダメージ量</param>
void PlayerControllerScript::TakeDamage(int amount) {
    if (isDead_) return;

    hp_ -= amount;
    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
    }
}

void PlayerControllerScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.2f, 0.7f, 1.0f, 1.0f };
    constexpr ImVec4 kActionColor = { 0.4f, 0.9f, 0.5f, 1.0f };
    constexpr float kTestBtnWidth = 200.0f;
    constexpr float kTestBtnHeight = 26.0f;

    ImGui::TextColored(kHeaderColor, "【プレイヤーコントローラー設定】");
    ImGui::Separator();

    ImGui::DragInt("最大HP##MaxHP", &maxHp_, 1, kMinHpLimit, kMaxHpLimit);
    ImGui::ProgressBar(static_cast<float>(hp_) / static_cast<float>(maxHp_), ImVec2(0.0f, 0.0f),
        std::to_string(hp_).append(" / ").append(std::to_string(maxHp_)).c_str());

    ImGui::Checkbox("残弾無限(デバッグ)##InfiniteAmmo", &infiniteAmmo_);
    ImGui::DragInt("最大装弾数##MaxAmmo", &maxAmmo_, 1, kMinAmmoLimit, kMaxAmmoLimit);
    ImGui::Text("残弾数: %d / %d %s", ammo_, maxAmmo_, (reloadTimer_ > 0 ? "[Reloading...]" : ""));

    ImGui::DragInt("リロード時間(F)##ReloadTime", &reloadDuration_, 1, 1, 120);
    ImGui::DragInt("連射間隔(F)##ShotInterval", &shotInterval_, 1, 1, 60);
    ImGui::DragFloat("弾速##BulletSpeed", &bulletSpeed_, 0.05f, 0.1f, 5.0f, "%.2f");

    ImGui::Separator();
    ImGui::Checkbox("自動走行中##IsAutoMoving", &isAutoMoving_);
    ImGui::Text("状態: %s", isDead_ ? "撃墜(Dead)" : "健在(Alive)");

    ImGui::Spacing();
    ImGui::TextColored(kActionColor, "【射撃テスト】");
    if (ImGui::Button("テスト発射 (左クリック相当)##TestFireBtn", ImVec2(kTestBtnWidth, kTestBtnHeight))) {
        Fire();
    }
#endif
}
