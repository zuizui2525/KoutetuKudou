#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include <cmath>
#include <algorithm>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(EnemyAIScript)

namespace {
    constexpr Vector4 kFlashColor = { 1.0f, 1.0f, 0.2f, 1.0f }; // 通常被弾フラッシュ（黄色）
    constexpr Vector4 kCriticalFlashColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // クリティカル被弾フラッシュ（純白）
    constexpr float kCenterXThreshold = 0.3f; // 正面回り込み完了とみなすX距離誤差 (m)
    constexpr int kCriticalMultiplier = 2;    // ヘッドショット時のダメージ倍率
    constexpr float kEpsilon = 0.0001f;

    // HPバー描画用カラー定数 (RGBA: 0.0f - 1.0f)
    constexpr Vector4 kHpBarBgColor = { 10.0f / 255.0f, 15.0f / 255.0f, 22.0f / 255.0f, 220.0f / 255.0f };
    constexpr Vector4 kHpBarBorderColor = { 20.0f / 255.0f, 25.0f / 255.0f, 35.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kHpGreen = { 45.0f / 255.0f, 235.0f / 255.0f, 115.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kHpYellow = { 245.0f / 255.0f, 205.0f / 255.0f, 45.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kHpRed = { 245.0f / 255.0f, 55.0f / 255.0f, 55.0f / 255.0f, 240.0f / 255.0f };
    constexpr Vector4 kBossHpBarColor = { 220.0f / 255.0f, 50.0f / 255.0f, 240.0f / 255.0f, 245.0f / 255.0f };
    constexpr Vector4 kBossBorderColor = { 255.0f / 255.0f, 215.0f / 255.0f, 60.0f / 255.0f, 240.0f / 255.0f };

    constexpr float kNearClipW = 0.05f;
    constexpr float kHalf = 0.5f;
    constexpr float kScreenWidth = 1280.0f;
    constexpr float kScreenHeight = 720.0f;
    constexpr float kScreenCullMargin = 80.0f;
    constexpr float kYellowHpRatioThreshold = 0.5f;
    constexpr float kRedHpRatioThreshold = 0.2f;
}

EnemyAIScript::EnemyAIScript()
    : ScriptComponent("EnemyAIScript") {
}

EnemyAIScript::~EnemyAIScript() {
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void EnemyAIScript::EnsureHpBarVisuals() {
    if (!hpBgSprite_) {
        hpBgSprite_ = std::make_unique<SpriteObject>();
        hpBgSprite_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hpBgSprite_.get());
    }
    if (!hpFillSprite_) {
        hpFillSprite_ = std::make_unique<SpriteObject>();
        hpFillSprite_->Initialize();
        SceneHierarchy::GetInstance()->Unregister(hpFillSprite_.get());
    }
    for (int i = 0; i < 4; ++i) {
        if (!hpBorders_[i]) {
            hpBorders_[i] = std::make_unique<Line2DObject>();
            hpBorders_[i]->Initialize();
            SceneHierarchy::GetInstance()->Unregister(hpBorders_[i].get());
        }
    }
}

void EnemyAIScript::Draw() {
    if (isDead_ || !owner_ || !owner_->IsVisible()) return;

    // 敵の頭上ワールド座標
    Vector3 enemyPos = owner_->GetPosition();
    float headOffset = isBoss_ ? kBossHpBarYOffset : kNormalHpBarYOffset;
    Vector3 headWorldPos = { enemyPos.x, enemyPos.y + headOffset, enemyPos.z };

    auto* camMgr = CameraResource::GetCameraManager();
    BaseCamera* camera = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!camera) return;

    Matrix4x4 viewProj = Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    float w = headWorldPos.x * viewProj.m[0][3] + headWorldPos.y * viewProj.m[1][3] + headWorldPos.z * viewProj.m[2][3] + viewProj.m[3][3];
    if (w <= kNearClipW) return; // カメラ背後は非表示

    float invW = 1.0f / w;
    float ndcX = (headWorldPos.x * viewProj.m[0][0] + headWorldPos.y * viewProj.m[1][0] + headWorldPos.z * viewProj.m[2][0] + viewProj.m[3][0]) * invW;
    float ndcY = (headWorldPos.x * viewProj.m[0][1] + headWorldPos.y * viewProj.m[1][1] + headWorldPos.z * viewProj.m[2][1] + viewProj.m[3][1]) * invW;

    float screenX = (ndcX * kHalf + kHalf) * kScreenWidth;
    float screenY = (-ndcY * kHalf + kHalf) * kScreenHeight;

    if (screenX < -kScreenCullMargin || screenX > kScreenWidth + kScreenCullMargin ||
        screenY < -kScreenCullMargin || screenY > kScreenHeight + kScreenCullMargin) {
        return;
    }

    EnsureHpBarVisuals();

    // 距離に応じた適度なサイズ減衰（遠くは小さく、近くは大きく）
    float baseWidth = isBoss_ ? kBossBarWidth : kNormalBarWidth;
    float baseHeight = isBoss_ ? kBossBarHeight : kNormalBarHeight;
    float scale = std::clamp(kReferenceDistance / (std::max)(4.0f, w), kMinBarScale, kMaxBarScale);
    float barW = baseWidth * scale;
    float barH = baseHeight * scale;

    float barMinX = screenX - barW * kHalf;
    float barMinY = screenY - barH * kHalf;
    float barMaxX = barMinX + barW;
    float barMaxY = barMinY + barH;

    float hpRatio = (maxHp_ > 0) ? std::clamp(static_cast<float>(hp_) / static_cast<float>(maxHp_), 0.0f, 1.0f) : 0.0f;
    float fillW = barW * hpRatio;

    Vector4 fillColor = (hpRatio > kYellowHpRatioThreshold) ? kHpGreen :
                        ((hpRatio > kRedHpRatioThreshold) ? kHpYellow : kHpRed);
    if (isBoss_) {
        fillColor = kBossHpBarColor;
    }

    // 1. 背景描画
    hpBgSprite_->SetPosition({ barMinX, barMinY, 0.0f });
    hpBgSprite_->SetSize(barW, barH);
    hpBgSprite_->GetMaterialData()->color = kHpBarBgColor;
    hpBgSprite_->Update();
    hpBgSprite_->Draw("white", true);

    // 2. 残量バー描画
    if (fillW > 0.0f) {
        hpFillSprite_->SetPosition({ barMinX, barMinY, 0.0f });
        hpFillSprite_->SetSize(fillW, barH);
        hpFillSprite_->GetMaterialData()->color = fillColor;
        hpFillSprite_->Update();
        hpFillSprite_->Draw("white", true);
    }

    // 3. 外枠線描画
    Vector4 borderColor = isBoss_ ? kBossBorderColor : kHpBarBorderColor;
    hpBorders_[0]->SetPoints({ barMinX, barMinY }, { barMaxX, barMinY });
    hpBorders_[1]->SetPoints({ barMinX, barMaxY }, { barMaxX, barMaxY });
    hpBorders_[2]->SetPoints({ barMinX, barMinY }, { barMinX, barMaxY });
    hpBorders_[3]->SetPoints({ barMaxX, barMinY }, { barMaxX, barMaxY });
    for (int i = 0; i < 4; ++i) {
        hpBorders_[i]->SetThickness(kBorderThickness);
        hpBorders_[i]->GetMaterialData()->color = borderColor;
        hpBorders_[i]->Update();
        hpBorders_[i]->Draw("white", true);
    }
}

void EnemyAIScript::Awake() {
    hp_ = isBoss_ ? kDefaultBossHp : kDefaultNormalHp;
    maxHp_ = hp_;
    aiState_ = AiState::Approach;
    toCenterGauge_ = 0.0f;
    attackGauge_ = 0.0f;
    cooldownTimer_ = 0;
    hitFlashTimer_ = 0;
    isDead_ = false;
}

void EnemyAIScript::Start() {
    // メッシュレンダラーから初期カラーを取得
    if (owner_) {
        if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
            originalColor_ = mr->GetColor();
        }
    }
}

void EnemyAIScript::OnUpdate() {
    if (isDead_ || !owner_) {
        return;
    }

    // 被弾フラッシュの更新
    HandleHitFlash();

    // プレイヤーの自動検索（未設定の場合）
    if (!targetObject_) {
        auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                    targetObject_ = obj.get();
                    targetUUID_ = obj->GetUUID();
                    break;
                }
            }
        }
    }

    // AIの更新
    UpdateAI();
}

/// <summary>
/// 敵キャラクターの自機追従、視線回転、および5段階のAIステートマシンを更新します。
/// 
/// 【ステート遷移フロー】
/// 1. カリング判定: 自機より後方（kCullDistanceBack）に置き去りにされた場合は即座に消滅。
/// 2. 視線追従: 自機の3Dワールド座標に向けて Yaw・Pitch 回転を同期。
/// 3. ステートマシン:
///    - Approach: 自機後方から等速前進。視界内（kApproachTransitionOffset）に入ると TargetLock へ遷移。
///    - TargetLock: 自機と並走しながらロックオンゲージ（toCenterGauge_）を蓄積。満タンで MoveToCenter へ遷移。
///    - MoveToCenter: プレイヤーのX軸正面へ回り込む。誤差が kCenterXThreshold 以内になると AttackCharge へ遷移。
///    - AttackCharge: 攻撃チャージゲージ（attackGauge_）を蓄積。満タンで UnavoidableAttack へ遷移。
///    - UnavoidableAttack: FireBullet() で自機へ向けて弾丸を発射後、Approach ステートへ復帰。
/// </summary>
void EnemyAIScript::UpdateAI() {
    if (!targetObject_) return;

    // 自機が走行戦闘中でない場合（ルート描画フェーズ中など）はAI行動（接近・攻撃）を停止
    auto* playerScript = targetObject_->GetComponent<PlayerControllerScript>();
    if (!playerScript || !playerScript->IsAutoMoving()) {
        return;
    }

    Vector3 pos = owner_->GetPosition();
    Vector3 playerPos = targetObject_->GetPosition();

    // 1. カリング判定（自機より後方に置き去りにされたら消滅）
    if (playerPos.z - pos.z > kCullDistanceBack) {
        isDead_ = true;
        owner_->SetVisible(false);
        return;
    }

    // 2. プレイヤーの方向を向く（ヨー・ピッチ回転）
    Vector3 toPlayer = { playerPos.x - pos.x, playerPos.y - pos.y, playerPos.z - pos.z };
    float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    float yaw = std::atan2(toPlayer.x, toPlayer.z);
    float pitch = -std::atan2(toPlayer.y, distXZ > kEpsilon ? distXZ : kEpsilon);
    owner_->SetRotate({ pitch, yaw, 0.0f });

    // 3. ステートマシンと移動制御
    // 自機前方の理想位置（停止距離）を追従し、自機に近づきすぎないよう重なりを防止
    float targetZ = playerPos.z + preferredDistanceZ_;
    float minLimitZ = playerPos.z + kMinDistanceZ;

    // 自機との絶対接近限界による重なり防止
    if (pos.z < minLimitZ) {
        pos.z = minLimitZ;
    }

    switch (aiState_) {
    case AiState::Approach: {
        // 自機前方の適正距離へ接近
        if (pos.z < targetZ) {
            pos.z += kApproachSpeedZ;
        } else {
            pos.z -= kDistanceAdjustSpeed;
        }

        if (pos.z >= playerPos.z + kApproachTransitionOffset) {
            aiState_ = AiState::TargetLock;
            toCenterGauge_ = 0.0f;
        }
        break;
    }
    case AiState::TargetLock: {
        // 並走しながら距離をキープし、中央移動チャージ
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        toCenterGauge_ += kToCenterGaugeSpeed;
        if (toCenterGauge_ >= 1.0f) {
            aiState_ = AiState::MoveToCenter;
        }
        break;
    }
    case AiState::MoveToCenter: {
        // 並走距離をキープしつつ、滑らかなLerpで自機のX正面へ回り込む
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        float diffX = playerPos.x - pos.x;
        pos.x += diffX * kMoveLerpFactorX;

        if (std::abs(diffX) <= kCenterXThreshold) {
            aiState_ = AiState::AttackCharge;
            attackGauge_ = 0.0f;
        }
        break;
    }
    case AiState::AttackCharge: {
        // 攻撃発射前のチャージ蓄積
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        attackGauge_ += kAttackGaugeSpeed;
        if (attackGauge_ >= 1.0f) {
            aiState_ = AiState::UnavoidableAttack;
        }
        break;
    }
    case AiState::UnavoidableAttack: {
        // 弾丸発射
        FireBullet();
        aiState_ = AiState::Cooldown;
        cooldownTimer_ = cooldownDuration_;
        break;
    }
    case AiState::Cooldown: {
        // 攻撃後のクールタイム（距離を保って様子見・威嚇）
        if (pos.z < targetZ) pos.z += kDistanceAdjustSpeed;
        else if (pos.z > targetZ + 1.0f) pos.z -= kDistanceAdjustSpeed;

        if (cooldownTimer_ > 0) {
            cooldownTimer_--;
            if (cooldownTimer_ == 0) {
                // クールタイム満了で次の攻撃サイクルへ
                aiState_ = AiState::Approach;
                toCenterGauge_ = 0.0f;
                attackGauge_ = 0.0f;
            }
        }
        break;
    }
    }

    owner_->SetPosition(pos);
}

/// <summary>
/// 自身の前方座標から自機座標への方向ベクトルを算出し、敵弾プレハブ（Bullet）を射出します。
/// </summary>
void EnemyAIScript::FireBullet() {
    if (!owner_ || !targetObject_) return;

    Vector3 pos = owner_->GetPosition();
    Vector3 playerPos = targetObject_->GetPosition();

    Vector3 spawnPos = { pos.x, pos.y, pos.z + kBulletSpawnOffsetZ };
    Vector3 diff = { playerPos.x - spawnPos.x, playerPos.y - spawnPos.y, playerPos.z - spawnPos.z };
    float lenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    Vector3 dir = { 0.0f, 0.0f, -1.0f };
    if (lenSq > kEpsilon) {
        float invLen = 1.0f / std::sqrt(lenSq);
        dir = { diff.x * invLen, diff.y * invLen, diff.z * invLen };
    }

    GameObject* bulletObj = ScriptComponent::Instantiate("Bullet", spawnPos);
    if (bulletObj) {
        auto* bulletScript = bulletObj->GetComponent<BulletScript>();
        if (bulletScript) {
            bulletScript->SetDirection(dir);
            bulletScript->SetSpeed(kEnemyBulletSpeed);
            bulletScript->SetDamage(bulletDamage_);
            bulletScript->SetIsPlayerBullet(false); // 敵弾
        }
    }
}

/// <summary>
/// 自機弾からのダメージを適用し、頭部への命中の場合はダメージ倍率を反映します。
/// 被弾フラッシュ演出（黄色）を開始し、HPが0に達した場合は撃破（非表示）状態へ移行します。
/// </summary>
/// <param name="amount">基礎ダメージ量</param>
/// <param name="isCritical">弱点部位判定</param>
void EnemyAIScript::TakeDamage(int amount, bool isCritical) {
    if (isDead_) return;

    int finalDamage = isCritical ? amount * kCriticalMultiplier : amount;
    hp_ -= finalDamage;
    hitFlashTimer_ = kHitFlashDuration;

    if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
        mr->SetColor(isCritical ? kCriticalFlashColor : kFlashColor);
    }

    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        owner_->SetVisible(false);
    }
}

/// <summary>
/// 被弾時点滅タイマーをカウントダウンし、満了時にメッシュの色を元のマテリアルカラーへ戻します。
/// </summary>
void EnemyAIScript::HandleHitFlash() {
    if (hitFlashTimer_ > 0) {
        hitFlashTimer_--;
        if (hitFlashTimer_ == 0 && owner_) {
            if (auto* mr = owner_->GetComponent<MeshRendererComponent>()) {
                mr->SetColor(originalColor_);
            }
        }
    }
}

void EnemyAIScript::SetBoss(bool isBoss) {
    isBoss_ = isBoss;
    maxHp_ = isBoss ? kDefaultBossHp : kDefaultNormalHp;
    hp_ = maxHp_;
}

void EnemyAIScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 1.0f, 0.3f, 0.3f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【敵AIスクリプト設定】");
    ImGui::Separator();

    ImGui::Checkbox("ボス属性##IsBoss", &isBoss_);
    ImGui::DragInt("最大HP##MaxHP", &maxHp_, 1, 1, 100);
    ImGui::ProgressBar(static_cast<float>(hp_) / static_cast<float>(maxHp_), ImVec2(0.0f, 0.0f),
        std::to_string(hp_).append(" / ").append(std::to_string(maxHp_)).c_str());

    const char* stateNames[] = { "Approach", "TargetLock", "MoveToCenter", "AttackCharge", "UnavoidableAttack", "Cooldown" };
    int currentStateIndex = static_cast<int>(aiState_);
    ImGui::Text("AIステート: %s", (currentStateIndex >= 0 && currentStateIndex < 6) ? stateNames[currentStateIndex] : "Unknown");
    ImGui::ProgressBar(toCenterGauge_, ImVec2(0.0f, 0.0f), "LockOn Gauge");
    ImGui::ProgressBar(attackGauge_, ImVec2(0.0f, 0.0f), "Attack Gauge");
    if (aiState_ == AiState::Cooldown) {
        float cdProgress = static_cast<float>(cooldownTimer_) / static_cast<float>(cooldownDuration_);
        ImGui::ProgressBar(cdProgress, ImVec2(0.0f, 0.0f), ("Cooldown: " + std::to_string(cooldownTimer_)).c_str());
    }

    ImGui::Separator();
    ImGui::DragFloat("理想維持距離(Z)##PreferredDistZ", &preferredDistanceZ_, 0.5f, 5.0f, 30.0f, "%.1f m");
    ImGui::DragInt("クールタイム(F)##CooldownDuration", &cooldownDuration_, 5, 10, 300);
    ImGui::DragInt("敵弾攻撃力##EnemyDamage", &bulletDamage_, 1, 1, 10);
#endif
}
