#include "App/Scene/Game/Phase/PlayPhase.h"
#include "Engine/Input/Input.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "App/Scene/Game/Route.h"
#include "App/Scene/Game/Stage/Stage.h"
#include "App/Scene/Game/Player/Player.h"
#include "App/Scene/Game/Enemy/EnemyManager.h"
#include "App/Scene/Game/Camera/PlayCamera.h"
#include "App/Scene/Game/UI/Reticle.h"
#include "Engine/Graphics/Objects/Effect/Manager/EffectManager.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Base/WindowApp/WindowApp.h"

/**
 * @brief コンストラクタ
 * @param input キー入力・マウス入力のマネージャ
 * @param cameraMgr カメラマネージャ
 * @param route ルートデータクラス
 * @param stage ステージ障害物情報
 * @param player プレイヤーオブジェクト
 * @param enemyManager 敵の湧き・戦闘・衝突判定のマネージャ
 * @param playCamera 走行プレイ用のメイン追従カメラ
 * @param reticle 照準UIクラス
 * @param ioCurrentDistance 走行中距離（GameSceneのメンバ変数の参照）
 * @param onAreaCleared エリアのゴール到達時に呼び出されるコールバック関数
 */
PlayPhase::PlayPhase(
    Input* input,
    CameraManager* cameraMgr,
    Route* route,
    Stage* stage,
    Player* player,
    EnemyManager* enemyManager,
    PlayCamera* playCamera,
    Reticle* reticle,
    float& ioCurrentDistance,
    std::function<void()> onAreaCleared
) : input_(input),
    cameraMgr_(cameraMgr),
    route_(route),
    stage_(stage),
    player_(player),
    enemyManager_(enemyManager),
    playCamera_(playCamera),
    reticle_(reticle),
    currentDistance_(ioCurrentDistance),
    onAreaCleared_(onAreaCleared) {}

void PlayPhase::Initialize() {
    // プレイモード開始時に自動走行フラグを立てる
    player_->SetAutoMoving(true);
}

void PlayPhase::Update() {
    // 1. 自動走行位置同期
    currentDistance_ += Player::GetAutoSpeed();
    if (currentDistance_ >= route_->GetTotalDistance()) {
        currentDistance_ = route_->GetTotalDistance();
        player_->SetAutoMoving(false);

        // ゴール到達時のエリア遷移処理
        int currentArea = route_->GetCurrentAreaIndex();
        if (currentArea < kMaxAreaIndex) {
            if (onAreaCleared_) {
                onAreaCleared_(); // エリアクリアコールバック呼び出し
            }
            return;
        } else {
            // 最終エリアのゴール到達時にクリアシーンへ
            SceneManager::GetInstance()->ChangeScene(kClearSceneName);
            return;
        }
    }

    // 2. プレイヤーの位置・回転同期
    Vector3 playerPos = route_->GetPositionAtDistance(currentDistance_);
    Vector3 tangent = route_->GetTangentAtDistance(currentDistance_);
    Vector3 rot = route_->GetRotationAtDistance(currentDistance_);

    player_->SetPosition(playerPos);
    player_->SetRotation(rot);
    player_->SetDirection(tangent);

    // 3. 敵の動的湧き・戦闘判定・衝突判定の全更新
    enemyManager_->Update(player_);

    // 4. プレイ用カメラの更新
    playCamera_->Update(input_, playerPos, tangent, currentDistance_);

    // 5. プレイ中の各種オブジェクトの行列更新
    route_->UpdateSpheres();
    player_->UpdateWeapon(playCamera_->GetCamera()->GetViewMatrix());

    // 6. 雨エフェクトの追従
    if (auto rainEffect = EffectManager::GetInstance()->GetEffect(kRainEffectName)) {
        rainEffect->GetTransform().translate = playCamera_->GetCamera()->GetPosition();
    }

    // 7. プレイヤー（自機）の更新
    player_->Update();

    // 8. 照準（レティクル）の更新
    reticle_->Update();

    // 9. マップステージの更新
    stage_->Update();

    // 10. エフェクトシステムの更新
    EffectManager::GetInstance()->Update();
}

void PlayPhase::Draw() {
    Vector3 playerPos = player_->GetPosition();

    // 1. 3Dマップステージの描画
    stage_->Draw(false, true, playerPos);

    // 2. プレイヤー（自機）および武器・弾の描画
    player_->Draw();

    // 3. エネミーの描画（60.0fカリング付き）
    enemyManager_->Draw(playerPos);

    // 4. エフェクトの描画
    EffectManager::GetInstance()->Draw();

    // 5. スタート/ゴール球体の描画
    route_->DrawSpheres();

    // 6. 照準（レティクル）の描画
    reticle_->Draw();
}

void PlayPhase::ImGuiControl() {
    // プレイフェーズ中は特有のImGui表示は行いませんが、インターフェースの実装として空定義します
}
