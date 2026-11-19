#include "Scripts/Stage/RestraintTrapScript.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#include "Scripts/Player/PlayerControllerScript.h"
#include "Scripts/Enemy/EnemyAIScript.h"
#include "Scripts/Stage/MapRendererScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/ScriptRegistry.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
#include "Engine/Graphics/Objects/3d/Cylinder/CylinderObject.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include <algorithm>
#include <cmath>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

REGISTER_SCRIPT(RestraintTrapScript)

std::vector<RestraintTrapScript*> RestraintTrapScript::sActiveTraps_;

namespace {
    const std::string kAlphaPsoKey = "Object3D_Alpha";
    const std::string kWhiteTexKey = "white";
    const std::string kRushEnemyPrefabName = "Enemy";
    constexpr int kUnlitLightingMode = 0;
    constexpr float kPulseCycleMax = 1.0f;
    constexpr float kActivePulseMultiplier = 2.2f;
    constexpr float kCageHalfRatio = 0.5f;
    constexpr float kDegToRad = 3.1415926535f / 180.0f;
    constexpr float kDirEpsilon = 0.0001f;

    // 360度包囲配列（前方右→前方左→背後右死角→側方左死角→正面奥→右真横→左真横→真後ろ）
    constexpr float kRushRelativeAnglesDeg[] = {
        35.0f, -40.0f, 135.0f, -120.0f, 0.0f, 90.0f, -90.0f, 180.0f
    };
    constexpr size_t kRushAnglePatternCount = sizeof(kRushRelativeAnglesDeg) / sizeof(kRushRelativeAnglesDeg[0]);

    // 建物回避用の角度探索オフセット (度)
    constexpr float kSafeAngleOffsetsDeg[] = {
        0.0f, 25.0f, -25.0f, 50.0f, -50.0f, 80.0f, -80.0f, 115.0f, -115.0f
    };
}

RestraintTrapScript::RestraintTrapScript()
    : ScriptComponent("RestraintTrapScript") {
    sActiveTraps_.push_back(this);
}

RestraintTrapScript::~RestraintTrapScript() {
    auto it = std::find(sActiveTraps_.begin(), sActiveTraps_.end(), this);
    if (it != sActiveTraps_.end()) {
        sActiveTraps_.erase(it);
    }

    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }

    groundDisc_.reset();
    outerRing_.reset();
    innerPulseRing_.reset();
    floatingLockRing_.reset();
    energyCage_.reset();
}

void RestraintTrapScript::ResetAllTraps() {
    for (auto* trap : sActiveTraps_) {
        if (trap) {
            trap->ResetTrap();
        }
    }
}

void RestraintTrapScript::ResetTrap() {
    state_ = TrapState::Armed;
    activeTimerFrames_ = 0;
    visualPulseTimer_ = 0.0f;
    disarmFadeAlpha_ = kFullVisibleAlpha;
    rushSpawnedCount_ = 0;
    rushSpawnCooldown_ = 0;
    rushBaseForwardYaw_ = 0.0f;
    if (triggerRushOnActivate_ && autoCalcDurationByEnemyCount_) {
        restraintDurationSec_ = static_cast<float>((std::max)(1, rushEnemyCount_)) * durationPerEnemySec_;
    }
}

float RestraintTrapScript::GetEffectiveRestraintDurationSec() const {
    if (triggerRushOnActivate_ && autoCalcDurationByEnemyCount_) {
        return static_cast<float>((std::max)(1, rushEnemyCount_)) * durationPerEnemySec_;
    }
    return restraintDurationSec_;
}

void RestraintTrapScript::SetDurationPerEnemySec(float secPerEnemy) {
    durationPerEnemySec_ = std::clamp(secPerEnemy, kMinDurationPerEnemySec, kMaxDurationPerEnemySec);
    if (triggerRushOnActivate_ && autoCalcDurationByEnemyCount_) {
        restraintDurationSec_ = static_cast<float>((std::max)(1, rushEnemyCount_)) * durationPerEnemySec_;
    }
}

void RestraintTrapScript::SetRushEnemyCount(int count) {
    rushEnemyCount_ = std::clamp(count, kMinRushEnemyCount, kMaxRushEnemyCount);
    if (triggerRushOnActivate_ && autoCalcDurationByEnemyCount_) {
        restraintDurationSec_ = static_cast<float>((std::max)(1, rushEnemyCount_)) * durationPerEnemySec_;
    }
}

Vector4 RestraintTrapScript::GetMapColor() const {
    return (state_ == TrapState::Active) ? kActiveRingColor : kArmedRingColor;
}

void RestraintTrapScript::Awake() {
    ResetTrap();
    EnsureVisuals();
}

void RestraintTrapScript::Start() {
    EnsureVisuals();
}

void RestraintTrapScript::EnsureVisuals() {
    if (!groundDisc_) {
        groundDisc_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kDiscInnerRadius);
        groundDisc_->SetAutoRegisterHierarchy(false);
        groundDisc_->Initialize(kUnlitLightingMode);
    }
    if (!outerRing_) {
        outerRing_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kOuterRingInnerRadius);
        outerRing_->SetAutoRegisterHierarchy(false);
        outerRing_->Initialize(kUnlitLightingMode);
    }
    if (!innerPulseRing_) {
        innerPulseRing_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kPulseRingInnerRadius);
        innerPulseRing_->SetAutoRegisterHierarchy(false);
        innerPulseRing_->Initialize(kUnlitLightingMode);
    }
    if (!floatingLockRing_) {
        floatingLockRing_ = std::make_unique<FlatRingObject>(kRingDivideCount, kUnitBaseRadius, kFloatingRingInnerRadius);
        floatingLockRing_->SetAutoRegisterHierarchy(false);
        floatingLockRing_->Initialize(kUnlitLightingMode);
    }
    if (!energyCage_) {
        energyCage_ = std::make_unique<CylinderObject>();
        energyCage_->SetAutoRegisterHierarchy(false);
        energyCage_->SetRadius(kUnitBaseRadius);
        energyCage_->SetHeight(kUnitBaseRadius);
        energyCage_->SetSubdivision(kCageSubdivisions);
        energyCage_->Initialize(kUnlitLightingMode);
    }
}

void RestraintTrapScript::OnUpdate() {
    // 視覚アニメーションタイマー更新
    float speedMul = (state_ == TrapState::Active) ? kActivePulseMultiplier : 1.0f;
    visualPulseTimer_ += kPulseSpeed * speedMul;
    if (visualPulseTimer_ >= kPulseCycleMax) {
        visualPulseTimer_ -= kPulseCycleMax;
    }
    cageRotationAngle_ += kCageRotationSpeed * speedMul;
    floatingBobTimer_ += kFloatingRingBobSpeed * speedMul;

    auto* gamePhase = GamePhaseControllerScript::GetInstance();
    if (!gamePhase || !owner_) return;

    // レベルアップ3択選択中はトラップのタイマー・ラッシュ連続湧きを一時停止
    if (gamePhase->IsLevelUpChoosing()) {
        return;
    }

    // 走行戦闘フェーズ以外、または別エリアのトラップは判定しない
    if (gamePhase->GetCurrentPhase() != GamePhaseControllerScript::Phase::Play) {
        return;
    }
    if (gamePhase->GetCurrentArea() != targetArea_) {
        return;
    }

    if (state_ == TrapState::Disarmed) {
        if (disarmFadeAlpha_ > 0.0f) {
            disarmFadeAlpha_ = (std::max)(0.0f, disarmFadeAlpha_ - kDisarmFadeOutSpeed);
        }
        return;
    }

    if (state_ == TrapState::Active) {
        // 作動後は円・檻の透明度を徐々に下げて滑らかにフェードアウト消滅させる
        if (disarmFadeAlpha_ > 0.0f) {
            disarmFadeAlpha_ = (std::max)(0.0f, disarmFadeAlpha_ - kDisarmFadeOutSpeed);
        }

        // 拘束連動エネミーラッシュ（時間差連続湧き）の進行
        if (triggerRushOnActivate_) {
            UpdateRushWave();
        }

        activeTimerFrames_--;
        if (activeTimerFrames_ <= 0 || !gamePhase->IsRestrained()) {
            state_ = TrapState::Disarmed;
        }
        return;
    }

    // プレイヤーオブジェクトのキャッシュ探索
    if (!cachedPlayer_) {
        auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
        if (scene) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && (obj->GetName() == "Player" || obj->GetComponent<PlayerControllerScript>())) {
                    cachedPlayer_ = obj.get();
                    break;
                }
            }
        }
    }
    if (!cachedPlayer_) return;

    // XZ平面での円内侵入判定
    Vector3 trapPos = owner_->GetPosition();
    Vector3 playerPos = cachedPlayer_->GetPosition();
    float dx = playerPos.x - trapPos.x;
    float dz = playerPos.z - trapPos.z;
    float distSq = dx * dx + dz * dz;
    float radiusSq = trapRadius_ * trapRadius_;

    if (distSq <= radiusSq) {
        state_ = TrapState::Active;
        float effectiveDurationSec = GetEffectiveRestraintDurationSec();
        restraintDurationSec_ = effectiveDurationSec;
        activeTimerFrames_ = static_cast<int>(effectiveDurationSec * kTargetFps);

        // 発動時のプレイヤー進行方位角を基準として記録（360度包囲配列の基準軸）
        rushBaseForwardYaw_ = 0.0f;
        if (auto* playerScript = cachedPlayer_->GetComponent<PlayerControllerScript>()) {
            Vector3 fwd = playerScript->GetDirection();
            if (std::abs(fwd.x) > kDirEpsilon || std::abs(fwd.z) > kDirEpsilon) {
                rushBaseForwardYaw_ = std::atan2(fwd.x, fwd.z);
            }
        }

        rushSpawnedCount_ = 0;
        rushSpawnCooldown_ = 0;

        gamePhase->TriggerRestraint(effectiveDurationSec, triggerRushOnActivate_);

        // ラッシュ連動が有効なら1体目を即座に前方斜めへ出現させ、以降テンポよく連続湧き
        if (triggerRushOnActivate_ && rushEnemyCount_ > 0) {
            SpawnSingleRushEnemy(rushSpawnedCount_);
            rushSpawnedCount_++;
            rushSpawnCooldown_ = kRushSpawnIntervalFrames;
        }
    }
}

void RestraintTrapScript::UpdateRushWave() {
    if (rushSpawnedCount_ >= rushEnemyCount_) {
        return;
    }

    if (rushSpawnCooldown_ > 0) {
        rushSpawnCooldown_--;
        return;
    }

    SpawnSingleRushEnemy(rushSpawnedCount_);
    rushSpawnedCount_++;
    rushSpawnCooldown_ = kRushSpawnIntervalFrames;
}

bool RestraintTrapScript::FindSafeRushSpawnPosition(
    const Vector3& playerPos, float desiredAngleRad,
    Vector3& outPos, float& outAngleRad) const {

    const float candidateDistances[] = { kRushSpawnDistanceFar, kRushSpawnDistanceNear };

    for (float dist : candidateDistances) {
        for (float offsetDeg : kSafeAngleOffsetsDeg) {
            float testAngle = desiredAngleRad + offsetDeg * kDegToRad;
            Vector3 candidate = {
                playerPos.x + std::sin(testAngle) * dist,
                kRushEnemyGroundY,
                playerPos.z + std::cos(testAngle) * dist
            };

            // 1. スポーン地点そのものが建物に埋まっていないかチェック
            if (MapRendererScript::IsInsideBuilding(candidate, kRushBuildingSafeMargin)) {
                continue;
            }

            // 2. 自機からスポーン地点までの射線に建物が遮っていないかサンプリング判定
            bool hasLos = true;
            for (int s = 1; s < kRushLosSampleSteps; ++s) {
                float t = static_cast<float>(s) / static_cast<float>(kRushLosSampleSteps);
                Vector3 samplePt = {
                    playerPos.x + (candidate.x - playerPos.x) * t,
                    kRushEnemyGroundY,
                    playerPos.z + (candidate.z - playerPos.z) * t
                };
                if (MapRendererScript::IsInsideBuilding(samplePt, 0.4f)) {
                    hasLos = false;
                    break;
                }
            }

            if (hasLos) {
                outPos = candidate;
                outAngleRad = testAngle;
                return true;
            }
        }
    }

    // 万が一の狭い空間フォールバック（壁押し戻しを必ず適用）
    Vector3 fallback = {
        playerPos.x + std::sin(desiredAngleRad) * kRushSpawnDistanceNear,
        kRushEnemyGroundY,
        playerPos.z + std::cos(desiredAngleRad) * kRushSpawnDistanceNear
    };
    Vector3 pushout = MapRendererScript::GetBuildingPushout(fallback, kRushBuildingSafeMargin);
    fallback.x += pushout.x;
    fallback.z += pushout.z;
    outPos = fallback;
    outAngleRad = desiredAngleRad;
    return false;
}

void RestraintTrapScript::SpawnSingleRushEnemy(int waveIndex) {
    if (!cachedPlayer_) return;

    Vector3 playerPos = cachedPlayer_->GetPosition();
    size_t angleIdx = static_cast<size_t>(waveIndex) % kRushAnglePatternCount;
    float desiredAngle = rushBaseForwardYaw_ + kRushRelativeAnglesDeg[angleIdx] * kDegToRad;

    Vector3 spawnPos{};
    float finalAngle = desiredAngle;
    FindSafeRushSpawnPosition(playerPos, desiredAngle, spawnPos, finalAngle);

    GameObject* enemyGo = ScriptComponent::Instantiate(kRushEnemyPrefabName, spawnPos);
    if (enemyGo) {
        if (auto* enemyAI = enemyGo->GetComponent<EnemyAIScript>()) {
            enemyAI->SetEnemyType(EnemyAIScript::EnemyType::RushSurround);
            enemyAI->SetCustomMaxHp(kRushEnemyHp);
            enemyAI->SetSurroundAngle(finalAngle);
            enemyAI->SetSurroundStrafeDir((waveIndex % 2 == 0) ? 1.0f : -1.0f);
        }
    }

    // 転送出現のコンパクトなスパーク演出
    if (auto* effectMgr = EffectManager::GetInstance()) {
        effectMgr->PlayGPUEffect(spawnPos, kRushSpawnSparkCount);
    }
}

void RestraintTrapScript::Draw() {
    if (!owner_) return;

    auto* gamePhase = GamePhaseControllerScript::GetInstance();
    int activeArea = gamePhase ? gamePhase->GetCurrentArea() : 0;
    if (targetArea_ != activeArea) {
        return;
    }

    bool isPlayPhase = gamePhase && (gamePhase->GetCurrentPhase() == GamePhaseControllerScript::Phase::Play);
    // 走行中に作動してフェードアウトが完了したトラップは非表示
    if (isPlayPhase && state_ != TrapState::Armed && disarmFadeAlpha_ <= kMinVisibleAlpha) {
        return;
    }

    EnsureVisuals();
    UpdateAndDrawVisuals(isPlayPhase);
}

void RestraintTrapScript::UpdateAndDrawVisuals(bool isPlayPhase) {
    Vector3 pos = owner_->GetPosition();
    bool isTriggered = (state_ != TrapState::Armed);
    bool useActiveTheme = (isPlayPhase && isTriggered);
    float fadeMul = (isPlayPhase && isTriggered) ? disarmFadeAlpha_ : kFullVisibleAlpha;

    Vector4 discColor = useActiveTheme ? kActiveDiscColor : kArmedDiscColor;
    Vector4 ringColor = useActiveTheme ? kActiveRingColor : kArmedRingColor;
    Vector4 pulseColor = useActiveTheme ? kActivePulseColor : kArmedPulseColor;
    Vector4 cageColor = useActiveTheme ? kActiveCageColor : kArmedCageColor;
    Vector4 lockRingColor = useActiveTheme ? kActiveLockRingColor : kArmedLockRingColor;

    discColor.w *= fadeMul;
    ringColor.w *= fadeMul;
    pulseColor.w *= fadeMul;
    cageColor.w *= fadeMul;
    lockRingColor.w *= fadeMul;

    float bobWave = std::sin(floatingBobTimer_);

    // 1. 地面の半透明警告円盤
    if (groundDisc_) {
        groundDisc_->SetPosition({ pos.x, kTrapDiscElevation, pos.z });
        groundDisc_->SetRotate({ kHalfPi, 0.0f, 0.0f });
        groundDisc_->SetScale({ trapRadius_, trapRadius_, 1.0f });
        groundDisc_->SetColor(discColor);
        groundDisc_->Update();
        groundDisc_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }

    // 2. 外周くっきり警告リング
    if (outerRing_) {
        outerRing_->SetPosition({ pos.x, kTrapRingElevation, pos.z });
        outerRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });
        outerRing_->SetScale({ trapRadius_, trapRadius_, 1.0f });
        outerRing_->SetColor(ringColor);
        outerRing_->Update();
        outerRing_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }

    // 3. 外周から中心へ引き込む内向き警告パルスリング（敵エリアの外向き拡散と真逆の動き）
    if (innerPulseRing_) {
        float pulseScale = trapRadius_ * (kPulseCycleMax - visualPulseTimer_);
        if (pulseScale > kMinVisibleScale) {
            innerPulseRing_->SetPosition({ pos.x, kTrapPulseRingElevation, pos.z });
            innerPulseRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });
            innerPulseRing_->SetScale({ pulseScale, pulseScale, 1.0f });
            Vector4 dynamicPulseColor = pulseColor;
            dynamicPulseColor.w *= (kPulseCycleMax - visualPulseTimer_);
            innerPulseRing_->SetColor(dynamicPulseColor);
            innerPulseRing_->Update();
            innerPulseRing_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
        }
    }

    // 4. 上下にプレス脈動する2段目浮遊ロックリング（「閉じ込める檻」をシルエットと動きで直感表示）
    if (floatingLockRing_) {
        float lockBaseY = useActiveTheme ? kFloatingRingActiveBaseY : kFloatingRingPreviewBaseY;
        float lockY = lockBaseY + bobWave * kFloatingRingBobAmpY;
        floatingLockRing_->SetPosition({ pos.x, lockY, pos.z });
        floatingLockRing_->SetRotate({ kHalfPi, 0.0f, 0.0f });
        floatingLockRing_->SetScale({ trapRadius_, trapRadius_, 1.0f });
        floatingLockRing_->SetColor(lockRingColor);
        floatingLockRing_->Update();
        floatingLockRing_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }

    // 5. 呼吸脈動しながら回転するエネルギー檻シリンダー（作動フェード中またはルート手書き/待機時に表示）
    if (energyCage_ && (!isPlayPhase || (isTriggered && disarmFadeAlpha_ > kMinVisibleAlpha))) {
        float cageHeight = useActiveTheme
            ? (kCageActiveHeight + bobWave * kCagePreviewBobAmp)
            : (kCagePreviewHeight + bobWave * kCagePreviewBobAmp);
        energyCage_->SetPosition({ pos.x, cageHeight * kCageHalfRatio + kTrapDiscElevation, pos.z });
        energyCage_->SetScale({ trapRadius_, cageHeight, trapRadius_ });
        energyCage_->SetRotate({ 0.0f, cageRotationAngle_, 0.0f });
        energyCage_->SetColor(cageColor);
        energyCage_->Update();
        energyCage_->Draw(kWhiteTexKey, "", kAlphaPsoKey);
    }
}

void RestraintTrapScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.88f, 0.25f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【拘束停止円トラップ (Restraint Circle)】");
    ImGui::Separator();

    const char* stateLabel = (state_ == TrapState::Armed) ? "待機中 (Armed)" :
                             (state_ == TrapState::Active) ? "拘束発動中 (Active)" : "解除済み (Disarmed)";
    ImGui::Text("状態: %s", stateLabel);
    ImGui::SliderFloat("有効半径 (m)", &trapRadius_, kMinTrapRadius, kMaxTrapRadius, "%.1f m");
    ImGui::SliderInt("対象エリア (0-3)", &targetArea_, 0, 3);
    ImGui::Checkbox("発動時にエネミーラッシュ連動", &triggerRushOnActivate_);
    if (triggerRushOnActivate_) {
        if (ImGui::SliderInt("ラッシュ連続湧き数", &rushEnemyCount_, kMinRushEnemyCount, kMaxRushEnemyCount)) {
            SetRushEnemyCount(rushEnemyCount_);
        }
        ImGui::Checkbox("敵数から拘束時間を自動算出 (敵1体につき秒数指定)", &autoCalcDurationByEnemyCount_);
        if (autoCalcDurationByEnemyCount_) {
            if (ImGui::SliderFloat("敵1体あたりの拘束時間 (秒)", &durationPerEnemySec_, kMinDurationPerEnemySec, kMaxDurationPerEnemySec, "%.1f 秒/体")) {
                SetDurationPerEnemySec(durationPerEnemySec_);
            }
            restraintDurationSec_ = GetEffectiveRestraintDurationSec();
            ImGui::Text("合計拘束時間: %.1f 秒 (%d体 × %.1f秒)", restraintDurationSec_, rushEnemyCount_, durationPerEnemySec_);
        } else {
            ImGui::SliderFloat("拘束時間 (秒)", &restraintDurationSec_, kMinRestraintDurationSec, kMaxRestraintDurationSec, "%.1f 秒");
        }
        ImGui::Text("出現進捗: %d / %d 体", rushSpawnedCount_, rushEnemyCount_);
    } else {
        ImGui::SliderFloat("拘束時間 (秒)", &restraintDurationSec_, kMinRestraintDurationSec, kMaxRestraintDurationSec, "%.1f 秒");
    }

    if (ImGui::Button("トラップ状態をリセット")) {
        ResetTrap();
    }
#endif
}
