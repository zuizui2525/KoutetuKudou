#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Input/Input.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Audio/Audio.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(PlayerControllerScript)

namespace {
    constexpr float kDefaultPlayerMuzzleHeight = 1.0f; // カメラ未初期化時のフォールバック高さ
    constexpr float kRayTargetDistance = 50.0f;
    constexpr int kReloadKey = DIK_R;

    // 死亡演出用定数
    constexpr float kRandOffsetCenter = 0.5f;
    constexpr float kMeltSinkSpeed = 0.015f;
    constexpr int kBubbleInterval = 10;
    constexpr uint32_t kHitSmokeCount = 30;
    constexpr uint32_t kMeltBurstCount = 80;
    constexpr uint32_t kMeltStepCount = 20;
    constexpr float kMinSinkGroundY = 0.05f;
    constexpr float kMeltSmokeMaxProgress = 0.85f;
    constexpr float kMeltColorRedWeight = 0.3f;
    constexpr float kMeltColorBlueWeight = 0.4f;
    constexpr float kMeltColorTargetRed = 0.15f;
    constexpr float kMeltColorTargetBlue = 0.25f;
    constexpr float kMeltColorGreen = 0.02f;
}

PlayerControllerScript::PlayerControllerScript()
    : ScriptComponent("PlayerControllerScript") {
}

PlayerControllerScript::~PlayerControllerScript() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
    muzzleFlashDisc_.reset();
}

void PlayerControllerScript::Awake() {
    hp_ = maxHp_;
    ammo_ = maxAmmo_;
    reloadTimer_ = 0;
    shotIntervalTimer_ = 0;
    invincibilityTimer_ = 0;
    isDead_ = false;
    isAutoMoving_ = false;
    isMelting_ = false;
    meltTimer_ = 0;
    isInitialPosSaved_ = false;
    gunRecoilProgress_ = 0.0f;
    EnsureGunModel();
}

void PlayerControllerScript::Start() {
    if (owner_ && !isOriginalColorSaved_) {
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            originalColor_ = mr->GetColor();
            isOriginalColorSaved_ = true;
        }
    }
    if (owner_ && !isInitialScaleSaved_) {
        initialScale_ = owner_->GetScale();
        isInitialScaleSaved_ = true;
    }

    if (!isSeShootLoaded_) {
        if (auto* audio = Audio::GetInstance()) {
            seShoot_ = audio->LoadSound(kSeShootPath);
            isSeShootLoaded_ = true;
        }
    }
}

void PlayerControllerScript::OnUpdate() {
    // 死亡溶解演出の更新
    if (isMelting_) {
        UpdateDeathMeltAnimation();
        return;
    }

    if (isDead_ || !owner_) {
        return;
    }

    // 無敵時間タイマーおよび被弾点滅演出の更新
    if (invincibilityTimer_ > 0) {
        invincibilityTimer_--;
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            if (invincibilityTimer_ == 0) {
                mr->SetColor(originalColor_);
            } else {
                bool isFlash = ((invincibilityTimer_ / kDamageFlashCycle) % 2 == 0);
                Vector4 flashColor = { 1.0f, 0.2f, 0.2f, 0.5f }; // 被弾点滅（半透明赤色）
                mr->SetColor(isFlash ? flashColor : originalColor_);
            }
        }
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

    // マズルフラッシュの減衰
    if (muzzleFlashTimer_ > 0) {
        muzzleFlashTimer_--;
    }

    // 入力処理（リロード & 射撃: 走行中のみ実行可能とし、描画中の誤射を防止）
    if (isAutoMoving_) {
        HandleReload();
        HandleShooting();
    }

    // 手持ちGunモデルのTransform更新（カメラ追従およびリコイル演出）
    UpdateGunTransform();
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

    // 銃口先端の正確な3Dワールド座標およびレティクルのエイム目標地点を取得
    Vector3 muzzlePos = GetMuzzleWorldPosition();
    Vector3 targetPos = GetAimTargetPosition();

    // 銃口（muzzlePos）からエイム目標地点（targetPos）への発射ベクトルを計算
    Vector3 bulletDir = direction_;
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

    // プレハブからBulletを実体化（銃口位置から射出！）
    GameObject* bulletObj = ScriptComponent::Instantiate("Bullet", muzzlePos);
    if (bulletObj) {
        auto* bulletScript = bulletObj->GetComponent<BulletScript>();
        if (bulletScript) {
            bulletScript->SetDirection(bulletDir);
            bulletScript->SetSpeed(bulletSpeed_);
            bulletScript->SetIsPlayerBullet(true);
        }
        // 弾丸カラーをシックな漆黒（ガンメタルブラック）に設定
        if (auto* mr = bulletObj->GetComponent<MeshRendererComponent>()) {
            mr->SetColor(kPlayerBulletColor);
        }
    }

    // 射撃反動のカメラシェイク演出
    ShakeCamera(kShootShakeIntensity, kShootShakeDuration);

    // 銃手持ちモデルの反動（キックバック）演出をトリガー
    gunRecoilProgress_ = 1.0f;

    // マズルフラッシュ点火（銃口先端の瞬間閃光）
    muzzleFlashTimer_ = kMuzzleFlashDuration;
    muzzleFlashRotation_ = static_cast<float>(rand() % 360) * 0.0174533f;

    // 射撃効果音 (SE) の再生
    if (auto* audio = Audio::GetInstance()) {
        audio->PlaySoundW(seShoot_, kShootVolume, false);
    }
}

/// <summary>
/// 敵弾等からの被弾によるダメージをHPに減算し、HPが0に達した場合は撃墜（死亡・溶解演出）を開始します。
/// </summary>
/// <param name="amount">ダメージ量</param>
void PlayerControllerScript::TakeDamage(int amount) {
    if (isDead_ || invincibilityTimer_ > 0) return;

    hp_ -= amount;
    invincibilityTimer_ = kDefaultInvincibilityDuration;

    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        isMelting_ = true;
        meltTimer_ = 0;

        if (owner_) {
            if (!isInitialScaleSaved_) {
                initialScale_ = owner_->GetScale();
                isInitialScaleSaved_ = true;
            }
            if (!isInitialPosSaved_) {
                initialPos_ = owner_->GetPosition();
                isInitialPosSaved_ = true;
            }
            // 被弾直後は被弾煙を発生
            if (auto* effectMgr = EffectManager::GetInstance()) {
                effectMgr->PlayGPUEffect(initialPos_, kHitSmokeCount);
            }
        }
    }
}

/// <summary>
/// 敵にやられた際の「とけるような死亡演出」を更新します。
/// カメラが自機正面へ回り込む間は被弾硬直・微震動を行い、正面到達後に地面へドロッと潰れて液状化消滅します。
/// </summary>
void PlayerControllerScript::UpdateDeathMeltAnimation() {
    if (!owner_) return;

    meltTimer_++;

    // フェーズ1: カメラが自機正面へ回り込むまでの待機期間（被弾硬直・微震動）
    if (meltTimer_ < kCameraAlignFrames) {
        float rx = (static_cast<float>(rand()) / RAND_MAX - kRandOffsetCenter) * kDeathShakeIntensity;
        float rz = (static_cast<float>(rand()) / RAND_MAX - kRandOffsetCenter) * kDeathShakeIntensity;
        owner_->SetPosition({ initialPos_.x + rx, initialPos_.y, initialPos_.z + rz });

        // 被弾による赤みフラッシュ
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            bool isFlash = ((meltTimer_ / kDamageFlashCycle) % 2 == 0);
            Vector4 flashColor = { 1.0f, 0.3f, 0.3f, 1.0f };
            mr->SetColor(isFlash ? flashColor : originalColor_);
        }
        return;
    }

    // カメラが正面に到達した瞬間に本格溶解のバーストエフェクトを発生
    if (meltTimer_ == kCameraAlignFrames) {
        owner_->SetPosition(initialPos_);
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(initialPos_, kMeltBurstCount);
        }
    }

    // フェーズ2: 正面アングルでの本格溶解演出
    float meltProgress = static_cast<float>(meltTimer_ - kCameraAlignFrames) /
        static_cast<float>(kDeathMeltDuration - kCameraAlignFrames);
    meltProgress = std::clamp(meltProgress, 0.0f, 1.0f);

    // 1. スケール変形（Y軸がペシャンコに潰れ、XZ軸が液状に広がる）
    float scaleY = initialScale_.y * (1.0f - meltProgress * kMeltVerticalCollapseFactor);
    float scaleXZ = 1.0f + meltProgress * kMeltHorizontalSpreadFactor;
    owner_->SetScale({ initialScale_.x * scaleXZ, scaleY, initialScale_.z * scaleXZ });

    // 2. 重心の地面沈み込み（Y方向の低下）
    Vector3 pos = owner_->GetPosition();
    if (pos.y > kMinSinkGroundY) {
        pos.y -= kMeltSinkSpeed;
        owner_->SetPosition(pos);
    }

    // 3. マテリアル色の変化（黒紫色へ変色しつつフェードアウト）
    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
        float alpha = (1.0f - meltProgress);
        Vector4 meltColor = {
            originalColor_.x * (1.0f - meltProgress) * kMeltColorRedWeight + kMeltColorTargetRed * meltProgress,
            kMeltColorGreen,
            originalColor_.z * (1.0f - meltProgress) * kMeltColorBlueWeight + kMeltColorTargetBlue * meltProgress,
            alpha
        };
        mr->SetColor(meltColor);
    }

    // 4. 定期的な溶解泡パーティクルの噴出
    if ((meltTimer_ - kCameraAlignFrames) % kBubbleInterval == 0 && meltProgress < kMeltSmokeMaxProgress) {
        if (auto* effectMgr = EffectManager::GetInstance()) {
            effectMgr->PlayGPUEffect(owner_->GetPosition(), kMeltStepCount);
        }
    }

    // 5. 溶解完了時
    if (meltTimer_ >= kDeathMeltDuration) {
        isMelting_ = false;
        owner_->SetVisible(false); // 完全に溶けきって消滅
        owner_->SetPosition(initialPos_);
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
    if (invincibilityTimer_ > 0) {
        constexpr ImVec4 kInvColor = { 1.0f, 0.8f, 0.2f, 1.0f };
        ImGui::TextColored(kInvColor, "★ 無敵時間中: %d フレーム", invincibilityTimer_);
    }

    ImGui::Checkbox("残弾無限(デバッグ)##InfiniteAmmo", &infiniteAmmo_);
    ImGui::DragInt("最大装弾数##MaxAmmo", &maxAmmo_, 1, kMinAmmoLimit, kMaxAmmoLimit);
    ImGui::Text("残弾数: %d / %d %s", ammo_, maxAmmo_, (reloadTimer_ > 0 ? "[Reloading...]" : ""));

    ImGui::DragInt("リロード時間(F)##ReloadTime", &reloadDuration_, 1, 1, 120);
    ImGui::DragInt("連射間隔(F)##ShotInterval", &shotInterval_, 1, 1, 60);
    ImGui::DragFloat("弾速##BulletSpeed", &bulletSpeed_, 0.05f, 0.1f, 5.0f, "%.2f");
    ImGui::DragFloat("ルート走行速度##MoveSpeed", &moveSpeed_, 0.005f, kMinMoveSpeed, kMaxMoveSpeed, "%.3f");

    ImGui::Separator();
    ImGui::Checkbox("自動走行中##IsAutoMoving", &isAutoMoving_);
    ImGui::Text("状態: %s", isDead_ ? "撃墜(Dead)" : "健在(Alive)");

    ImGui::Spacing();
    ImGui::TextColored(kActionColor, "【射撃テスト】");
    if (ImGui::Button("テスト発射 (左クリック相当)##TestFireBtn", ImVec2(kTestBtnWidth, kTestBtnHeight))) {
        Fire();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "【手持ち武器 (Gun Viewmodel) 設定】");
    ImGui::DragFloat3("銃オフセット(右/下/前)##GunOffset", &gunOffset_.x, 0.01f, -2.0f, 2.0f, "%.2f m");
    ImGui::DragFloat3("銃回転角##GunRotation", &gunRotation_.x, 0.02f, -3.14f, 3.14f, "%.2f rad");
    ImGui::DragFloat3("銃スケール##GunScale", &gunScale_.x, 0.01f, 0.01f, 5.0f, "%.2f");
    ImGui::DragFloat3("銃口オフセット(X/Y/Z)##MuzzleOffset", &gunMuzzleOffset_.x, 0.01f, -2.0f, 3.0f, "%.2f m");
    ImGui::DragFloat2("照準補正(左/上)##AimCorrection", &aimCorrection_.x, 0.01f, -1.0f, 1.0f, "%.2f m");
    ImGui::DragFloat("ゼロイン距離##ZeroInDistance", &zeroInDistance_, 0.5f, 5.0f, 100.0f, "%.1f m");
    ImGui::DragFloat("フラッシュサイズ##FlashScale", &muzzleFlashScale_, 0.002f, 0.005f, 0.20f, "%.3f m");
    ImGui::DragFloat("フラッシュ前方位置##FlashForward", &muzzleFlashForwardOffset_, 0.005f, -0.1f, 0.3f, "%.3f m");
    ImGui::ColorEdit4("銃マテリアル色##GunColor", &gunColor_.x, ImGuiColorEditFlags_AlphaBar);
    if (ImGui::Button("銃設定初期化##ResetGunOffset")) {
        gunOffset_ = kGunDefaultOffset;
        gunRotation_ = kGunDefaultRotation;
        gunScale_ = kGunDefaultScale;
        gunColor_ = kGunDefaultColor;
        gunMuzzleOffset_ = kDefaultGunMuzzleOffset;
        aimCorrection_ = kDefaultAimCorrection;
        zeroInDistance_ = kDefaultZeroInDistance;
        muzzleFlashScale_ = kDefaultMuzzleFlashScale;
        muzzleFlashForwardOffset_ = kDefaultMuzzleFlashForwardOffset;
    }
#endif
}

void PlayerControllerScript::EnsureGunModel() {
    if (!gunModel_) {
        gunModel_ = std::make_unique<ModelObject>();
        gunModel_->SetAutoRegisterHierarchy(false);
        gunModel_->Initialize(2); // 陰影・ライティング有効
        gunModel_->SetColor(gunColor_);
        gunModel_->SetScale(gunScale_);
    }
    if (!muzzleFlashDisc_) {
        constexpr uint32_t kFlashDivideCount = 16;
        constexpr float kFlashBaseRadius = 1.0f;
        constexpr float kFlashInnerRadius = 0.0f; // 完全な面ディスク
        muzzleFlashDisc_ = std::make_unique<FlatRingObject>(kFlashDivideCount, kFlashBaseRadius, kFlashInnerRadius);
        muzzleFlashDisc_->SetAutoRegisterHierarchy(false);
        muzzleFlashDisc_->Initialize(0); // 非ライティング（鮮烈発光）
        muzzleFlashDisc_->SetColor(kMuzzleFlashColor);
    }
}

void PlayerControllerScript::UpdateGunTransform() {
    EnsureGunModel();
    if (!gunModel_) return;

    // リコイル回復 (スムーズに0へ収束)
    if (gunRecoilProgress_ > 0.001f) {
        gunRecoilProgress_ += (0.0f - gunRecoilProgress_) * kGunRecoilRecovery;
    } else {
        gunRecoilProgress_ = 0.0f;
    }

    // FPS視点用のアクティブカメラを取得
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();

    // カメラ回転行列から右・上・前基底ベクトルを計算
    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight   = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp      = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    Vector3 camForward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };

    // リコイルによる銃の手前への後退および銃口の跳ね上がり
    float recoilKick = gunRecoilProgress_ * kGunRecoilKickbackZ;
    float recoilPitch = gunRecoilProgress_ * kGunRecoilPitch;

    // カメラローカル座標からワールド座標へ変換
    Vector3 gunWorldPos = {
        camPos.x + camRight.x * gunOffset_.x + camUp.x * gunOffset_.y + camForward.x * (gunOffset_.z + recoilKick),
        camPos.y + camRight.y * gunOffset_.x + camUp.y * gunOffset_.y + camForward.y * (gunOffset_.z + recoilKick),
        camPos.z + camRight.z * gunOffset_.x + camUp.z * gunOffset_.y + camForward.z * (gunOffset_.z + recoilKick)
    };

    // レティクルのエイム目標地点に向けて銃を傾ける（エイム指向IK）
    Vector3 targetPos = GetAimTargetPosition();
    Vector3 aimDiff = {
        targetPos.x - gunWorldPos.x,
        targetPos.y - gunWorldPos.y,
        targetPos.z - gunWorldPos.z
    };
    float hDist = std::sqrt(aimDiff.x * aimDiff.x + aimDiff.z * aimDiff.z);
    float aimYaw = std::atan2(aimDiff.x, aimDiff.z);
    float aimPitch = std::atan2(-aimDiff.y, hDist);

    Vector3 gunWorldRot = {
        aimPitch + gunRotation_.x + recoilPitch,
        aimYaw + gunRotation_.y,
        camRot.z + gunRotation_.z
    };

    gunModel_->SetPosition(gunWorldPos);
    gunModel_->SetRotate(gunWorldRot);
    gunModel_->SetScale(gunScale_);
    gunModel_->SetColor(gunColor_);
    gunModel_->Update();

    // マズルフラッシュのTransform更新（銃口の真先・前方に突き出して小型配置）
    if (muzzleFlashDisc_ && muzzleFlashTimer_ > 0) {
        Vector3 muzzleWorldPos = GetMuzzleWorldPosition();

        // 銃の向いている前方方向（gunWorldRotに基づく前方単位ベクトル）
        Matrix4x4 gunRotMat = Math::MakeRotateMatrix(gunWorldRot.x, gunWorldRot.y, gunWorldRot.z);
        Vector3 gunForward = { gunRotMat.m[2][0], gunRotMat.m[2][1], gunRotMat.m[2][2] };

        // 銃口の真先に突き出した位置へ配置
        Vector3 flashPos = {
            muzzleWorldPos.x + gunForward.x * muzzleFlashForwardOffset_,
            muzzleWorldPos.y + gunForward.y * muzzleFlashForwardOffset_,
            muzzleWorldPos.z + gunForward.z * muzzleFlashForwardOffset_
        };

        float flashScale = muzzleFlashScale_ * (static_cast<float>(muzzleFlashTimer_) / static_cast<float>(kMuzzleFlashDuration));
        muzzleFlashDisc_->SetPosition(flashPos);
        muzzleFlashDisc_->SetScale({ flashScale, flashScale, 1.0f });
        muzzleFlashDisc_->SetRotate({ gunWorldRot.x, gunWorldRot.y, gunWorldRot.z + muzzleFlashRotation_ });
        muzzleFlashDisc_->SetColor(kMuzzleFlashColor);
        muzzleFlashDisc_->Update();
    }
}

Vector3 PlayerControllerScript::GetMuzzleWorldPosition() const {
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) {
        // カメラ未初期化時の安全なフォールバック
        if (owner_) {
            Vector3 pos = owner_->GetPosition();
            return { pos.x, pos.y + kDefaultPlayerMuzzleHeight, pos.z };
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();

    // カメラ回転行列から右・上・前基底ベクトルを計算
    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight   = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp      = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
    Vector3 camForward = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };

    // リコイルによる銃の手前への後退および銃口の跳ね上がり
    float recoilKick = gunRecoilProgress_ * kGunRecoilKickbackZ;
    float recoilPitch = gunRecoilProgress_ * kGunRecoilPitch;

    // カメラローカル座標からワールド座標へ変換
    Vector3 gunWorldPos = {
        camPos.x + camRight.x * gunOffset_.x + camUp.x * gunOffset_.y + camForward.x * (gunOffset_.z + recoilKick),
        camPos.y + camRight.y * gunOffset_.x + camUp.y * gunOffset_.y + camForward.y * (gunOffset_.z + recoilKick),
        camPos.z + camRight.z * gunOffset_.x + camUp.z * gunOffset_.y + camForward.z * (gunOffset_.z + recoilKick)
    };

    // レティクルのエイム目標地点に向けて銃を傾ける（エイム指向IK）
    Vector3 targetPos = GetAimTargetPosition();
    Vector3 aimDiff = {
        targetPos.x - gunWorldPos.x,
        targetPos.y - gunWorldPos.y,
        targetPos.z - gunWorldPos.z
    };
    float hDist = std::sqrt(aimDiff.x * aimDiff.x + aimDiff.z * aimDiff.z);
    float aimYaw = std::atan2(aimDiff.x, aimDiff.z);
    float aimPitch = std::atan2(-aimDiff.y, hDist);

    Vector3 gunWorldRot = {
        aimPitch + gunRotation_.x + recoilPitch,
        aimYaw + gunRotation_.y,
        camRot.z + gunRotation_.z
    };

    // 銃モデルの完全なワールドアフィン変換行列（Scale * Rotate * Translate）を構築
    Matrix4x4 gunWorldMatrix = Math::MakeAffineMatrix(gunScale_, gunWorldRot, gunWorldPos);

    // gun.obj の銃口先端ローカル座標（銃身外側へ抜けた位置）をワールド座標へ射影変換
    return Math::Transform(gunWorldMatrix, gunMuzzleOffset_);
}

Vector3 PlayerControllerScript::GetAimTargetPosition() const {
    auto* cameraMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = cameraMgr ? cameraMgr->GetActiveCamera() : nullptr;
    if (!camera) {
        if (owner_) {
            Vector3 pos = owner_->GetPosition();
            return { pos.x + direction_.x * zeroInDistance_, pos.y + kDefaultPlayerMuzzleHeight, pos.z + direction_.z * zeroInDistance_ };
        }
        return { 0.0f, 0.0f, zeroInDistance_ };
    }

    Vector3 camPos = camera->GetPosition();
    Vector3 camRot = camera->GetRotation();
    Matrix4x4 rotMat = Math::MakeRotateMatrix(camRot.x, camRot.y, camRot.z);
    Vector3 camRight = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
    Vector3 camUp    = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    float clientW = static_cast<float>(WindowApp::kClientWidth);
    float clientH = static_cast<float>(WindowApp::kClientHeight);
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 mousePos = GameViewWindow::GetMousePosition();

    Vector2 scaledMousePos = mousePos;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
        scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
    }

    Vector3 rayStart = camPos;
    Vector3 rayDir = direction_;
    camera->CreateRay(scaledMousePos, clientW, clientH, rayStart, rayDir);

    // 照準レイ前方 zeroInDistance_ をエイム目標地点の基準とする
    Vector3 rawTargetPos = {
        rayStart.x + rayDir.x * zeroInDistance_,
        rayStart.y + rayDir.y * zeroInDistance_,
        rayStart.z + rayDir.z * zeroInDistance_
    };

    // カメラ基準の左・上方向バイアス補正を加算（レティクル中心への完全アライメント）
    return {
        rawTargetPos.x + camRight.x * aimCorrection_.x + camUp.x * aimCorrection_.y,
        rawTargetPos.y + camRight.y * aimCorrection_.x + camUp.y * aimCorrection_.y,
        rawTargetPos.z + camRight.z * aimCorrection_.x + camUp.z * aimCorrection_.y
    };
}

void PlayerControllerScript::Draw() {
    if (isDead_ || isMelting_) return;

    // 走行戦闘中（isAutoMoving_ == true）の一人称視点時にのみGunを手持ち描画
    if (!isAutoMoving_) return;

    EnsureGunModel();
    if (gunModel_) {
        gunModel_->Draw(kGunModelKey, "white");
    }

    // マズルフラッシュ描画（射撃瞬間のみ点灯）
    if (muzzleFlashDisc_ && muzzleFlashTimer_ > 0) {
        muzzleFlashDisc_->Draw("white");
    }
}
