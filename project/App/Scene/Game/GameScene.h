#pragma once
#include "App/Scene/Core/IScene.h"
#include <memory>
#include <string>

class PostProcess;
class LineObject;
class Stage;
class IGamePhase;

// エンジンコンポーネントのインクルード
#include "Engine/Input/Input.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "App/Scene/Game/Light/GameLight.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Debug/DebugCamera.h"
#include "App/Scene/Game/Camera/DrawRouteCamera.h"
#include "App/Scene/Game/Camera/PlayCamera.h"
#include "App/Scene/Game/UI/Minimap.h"
#include "App/Scene/Game/UI/Reticle.h"
#include "App/Scene/Game/Enemy/EnemyManager.h"
#include "App/Scene/Game/Player/Player.h"
#include "App/Scene/Game/Enemy/Enemy.h"
#include "App/Scene/Game/Route.h"
#include "App/Scene/Game/Stage/StageEditor.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Square/SquareObject.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"

/**
 * @brief ゲーム本編を管理するシーンクラス
 */
class GameScene : public IScene {
public:
    // コンストラクタ・デストラクタ
    GameScene();
    ~GameScene() override;

    // IScene インターフェースの仮想関数オーバーライド
    void Initialize() override;     // 初期化処理
    void ImGuiControl() override;   // デバッグ用ImGui描画処理
    void Update() override;         // 毎フレーム更新処理
    void Draw() override;           // 毎フレーム描画処理

private:
    // ゲームモード定義
    enum class GameMode {
        DrawRoute, // ルート描画モード
        Play       // 走行プレイモード
    };

    // マジックナンバーを排除するための定数宣言
    static inline const std::string kMainCameraName = "Main";       // メインカメラの登録・選択用キー
    static inline const std::string kDebugCameraName = "Debug";     // デバッグカメラの登録・選択用キー
    static inline const std::string kClearSceneName = "Clear";       // クリアシーンへの遷移用キー
    static inline const std::string kGameOverSceneName = "GameOver"; // ゲームオーバーシーンへの遷移用キー
    
    // ルート描画・走行用の定数
    static inline const Vector3 kTopDownCameraPos = { 0.0f, 120.0f, 0.0f }; // 上空見下ろしカメラ位置（スタート・ゴールが画面内に確実に収まる高さ）
    static inline const Vector3 kTopDownCameraRot = { 1.57079f, 0.0f, 0.0f }; // 真下を向く回転 (90度)

private:
    // エンジンの各種マネージャへの生ポインタ（所有権は持たない）
    Input* input_ = nullptr;            // キー入力マネージャ
    CameraManager* cameraMgr_ = nullptr; // カメラ管理者
    LightManager* lightMgr_ = nullptr;  // ライト管理者
    PostProcess* postProcess_ = nullptr;

    // シーン内で作成・管理するオブジェクト
    // カメラ制御用オブジェクト
    std::unique_ptr<DrawRouteCamera> drawRouteCamera_; // ルート描画用カメラ
    std::unique_ptr<PlayCamera> playCamera_;           // 走行プレイ用カメラ
    std::shared_ptr<DebugCamera> debugCamera_;        // デバッグ確認用フリーカメラ
    std::unique_ptr<GameLight> light_;                  // ライト管理者
    std::unique_ptr<Player> player_;                // プレイヤーオブジェクト
    std::unique_ptr<EnemyManager> enemyManager_;    // 敵管理者
    std::unique_ptr<Reticle> reticle_;              // レティクルUI
    bool showRouteEditor_ = true;                   // Route Editorウィンドウの表示フラグ

    // リファクタリングによる新設コンポーネント
    std::unique_ptr<Route> route_;
    std::unique_ptr<StageEditor> stageEditor_;

    // ルート関連メンバ変数
    std::unique_ptr<IGamePhase> currentPhase_;       // 現在の進行フェーズ
    float currentDistance_ = 0.0f;                   // 現在の走行距離

    // マップステージ
    std::unique_ptr<Stage> stage_;

    // 2Dミニマップ
    std::unique_ptr<Minimap> minimap_;



    // 右画面用 3D インジケータ (ズームカメラ 3D 空間用)
    std::unique_ptr<SphereObject> cursorIndicatorZoom_;

    // フェーズ遷移処理
    void StartGame();
    void TransitionToPlay();
    void TransitionToDraw();

public:
    // エディタ表示フラグのポインタ取得ゲッター
    bool* GetShowStageEditorPtr() { return stageEditor_->GetShowEditorPtr(); }
    bool* GetShowRouteEditorPtr() { return &showRouteEditor_; }
private:

    // シェイク機能用変数と定数
    // 敵の有効化フラグ（不要になったら削除可能）
    bool isEnemyEnabled_ = true;
    static inline const Vector3 kDefaultCameraPos = { 0.0f, 4.0f, -20.0f }; // メインカメラの基準位置
    static inline const Vector3 kZoomCameraOffset = { 0.0f, 25.0f, -20.0f }; // ズームカメラの注視点からのオフセット
    static inline const float kZoomScrollSpeed = 0.15f;                      // ズームカメラの右ドラッグスクロール速度
    static inline const int kShakeDuration = 15;                             // シェイクフレーム数
    static inline const float kShakeIntensity = 0.2f;                        // シェイクの強さ
    static inline const std::string kRainEffectName = "WaterDrop";            // 雨のエフェクト名

    // カメラ首振り用定数
    static inline const float kCameraYawLimit = 0.35f;                       // 最大首振りヨー角（ラジアン）
    static inline const float kCameraYawSpeed = 0.015f;                      // 首振り速度
    static inline const float kCameraYawReturnSpeed = 0.02f;                 // 正面復帰速度

};

