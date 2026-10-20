#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Player/BulletScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include <cmath>
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
}

EnemyAIScript::EnemyAIScript()
    : ScriptComponent("EnemyAIScript") {
}

void EnemyAIScript::Awake() {
    hp_ = isBoss_ ? kDefaultBossHp : kDefaultNormalHp;
    maxHp_ = hp_;
    aiState_ = AiState::Approach;
    toCenterGauge_ = 0.0f;
    attackGauge_ = 0.0f;
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

    // 3. ステートマシン
    switch (aiState_) {
    case AiState::Approach: {
        pos.z += kApproachSpeedZ;
        if (pos.z > playerPos.z + kApproachTransitionOffset) {
            aiState_ = AiState::TargetLock;
            toCenterGauge_ = 0.0f;
        }
        break;
    }
    case AiState::TargetLock: {
        // 並走しながら中央移動チャージ
        toCenterGauge_ += kToCenterGaugeSpeed;
        if (toCenterGauge_ >= 1.0f) {
            aiState_ = AiState::MoveToCenter;
        }
        break;
    }
    case AiState::MoveToCenter: {
        // プレイヤーのX正面へ移動
        float diffX = playerPos.x - pos.x;
        if (std::abs(diffX) > kCenterXThreshold) {
            pos.x += (diffX > 0.0f ? 1.0f : -1.0f) * kMoveToCenterSpeedX;
        } else {
            aiState_ = AiState::AttackCharge;
            attackGauge_ = 0.0f;
        }
        break;
    }
    case AiState::AttackCharge: {
        attackGauge_ += kAttackGaugeSpeed;
        if (attackGauge_ >= 1.0f) {
            aiState_ = AiState::UnavoidableAttack;
        }
        break;
    }
    case AiState::UnavoidableAttack: {
        FireBullet();
        aiState_ = AiState::Approach;
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

    const char* stateNames[] = { "Approach", "TargetLock", "MoveToCenter", "AttackCharge", "UnavoidableAttack" };
    int currentStateIndex = static_cast<int>(aiState_);
    ImGui::Text("AIステート: %s", (currentStateIndex >= 0 && currentStateIndex < 5) ? stateNames[currentStateIndex] : "Unknown");
    ImGui::ProgressBar(toCenterGauge_, ImVec2(0.0f, 0.0f), "LockOn Gauge");
    ImGui::ProgressBar(attackGauge_, ImVec2(0.0f, 0.0f), "Attack Gauge");
#endif
}
