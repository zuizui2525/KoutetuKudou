#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>
#include <memory>
#include <vector>

class RouteManagerScript;
class PlayerControllerScript;
class EnemySpawnManagerScript;
class SpriteObject;
class Circle2DObject;
class Ring2DObject;
class Line2DObject;
class TextObject2D;
class TextTextureGenerator;

/// <summary>
/// ゲーム全体の進行フェーズ（DrawRoute ⇔ Play）、エリア切り替え、シーン遷移を統括するスクリプト
/// </summary>
class GamePhaseControllerScript : public ScriptComponent {
public:
    enum class Phase {
        DrawRoute, // ルート手書きフェーズ
        Play       // 走行戦闘フェーズ
    };

    GamePhaseControllerScript();
    ~GamePhaseControllerScript() override;

    void Awake() override;
    void Start() override;
    void OnUpdate() override;
    void Draw2D() override;
    void OnGUI() override;
    void OnInspectorGUI() override;

    void TransitionToDraw();
    void TransitionToPlay();

    Phase GetCurrentPhase() const { return currentPhase_; }
    int GetCurrentArea() const { return currentArea_; }

    float GetPlayerMoveSpeed() const { return playerMoveSpeed_; }
    void SetPlayerMoveSpeed(float speed) { playerMoveSpeed_ = speed; }

private:
    void UpdateDrawRoutePhase();
    void UpdatePlayPhase();
    void HandleAreaClear();
    void DrawHUD();
    void Draw2DMap(const Vector2& viewPosMin, const Vector2& viewSize);
    void DrawGoSign(const Vector2& viewPosMin, const Vector2& viewSize);
    void ClearAllEnemies();
    void SetReticleVisible(bool visible);

    // DirectX 12 ベースの 2D オブジェクト描画メソッド
    void Init2DObjects();
    void Render2DMapDirectX();
    void RenderGoSignDirectX();

private:
    // マジックナンバー排除のための定数
    static constexpr int kMaxAreas = 4;
    static constexpr float kDefaultPlayerMoveSpeed = 0.15f; // 毎フレームの標準走行速度（適正値）
    static constexpr float kMinPlayerMoveSpeed = 0.01f;     // 最小速度
    static constexpr float kMaxPlayerMoveSpeed = 2.0f;      // 最大速度
    static constexpr float kDrawCameraHeight = 65.0f;     // 斜め立体俯瞰のカメラ高さ
    static constexpr float kDrawCameraZOffset = -18.0f;   // スタート地点手前のオフセット
    static constexpr float kDrawCameraPitch = 0.98f;      // 約56度の斜め見下ろし角度
    static inline const std::string kClearSceneName = "Clear";
    static inline const std::string kGameOverSceneName = "GameOver";
    static inline const std::string kReticleObjectName = "ReticleUI";

    // 2D マップパネルの定数（画面左下に大きく見やすく配置）
    static constexpr float kMapPanelWidth = 300.0f;       // マップ幅 (px)
    static constexpr float kMapPanelHeight = 540.0f;      // マップ高さ (px)
    static constexpr float kMapPanelMarginLeft = 32.0f;   // 左マージン (px)
    static constexpr float kMapPanelMarginBottom = 32.0f; // 下マージン (px)
    static constexpr float kMapInnerPadding = 16.0f;      // パネル内側余白 (px)
    static constexpr float kCircleDrawRadius = 10.0f;     // スタート・ゴールの2D表示半径 (px)
    static constexpr float kPlayerIconRadius = 9.0f;      // 自機アイコンの2D表示半径 (px)

    // 2D マップ内ミニリセットボタンの定数
    static constexpr float kMapResetBtnWidth = 64.0f;     // 2Dマップ内リセットボタン幅 (px)
    static constexpr float kMapResetBtnHeight = 24.0f;    // 2Dマップ内リセットボタン高さ (px)
    static constexpr float kMapResetBtnMarginRight = 14.0f;// 右マージン (px)
    static constexpr float kMapResetBtnMarginTop = 10.0f; // 上マージン (px)

    // GOサインパネルの定数
    static constexpr float kGoButtonWidth = 280.0f;       // GOボタン幅 (px)
    static constexpr float kGoButtonHeight = 54.0f;       // GOボタン高さ (px)
    static constexpr float kGoButtonMarginBottom = 30.0f; // 画面下からのマージン (px)
    static constexpr float kResetButtonWidth = 140.0f;    // リセットボタン幅 (px)
    static constexpr float kResetButtonHeight = 32.0f;    // リセットボタン高さ (px)
    static constexpr float kResetBtnSpacing = 8.0f;       // リセットボタンとGOボタンの間隔 (px)

    // メンバ変数
    Phase currentPhase_ = Phase::DrawRoute;
    int currentArea_ = 0;
    float currentDistance_ = 0.0f;
    float playerMoveSpeed_ = kDefaultPlayerMoveSpeed;

    RouteManagerScript* routeMgr_ = nullptr;
    PlayerControllerScript* playerScript_ = nullptr;
    EnemySpawnManagerScript* spawnMgr_ = nullptr;

    // DirectX 12 2D 描画オブジェクト
    bool is2DInitialized_ = false;
    float pulseAnimTimer_ = 0.0f;

    // 2D マップ用パーツ
    std::unique_ptr<SpriteObject> mapBgSprite_;
    std::unique_ptr<SpriteObject> mapInnerSprite_;
    std::unique_ptr<Line2DObject> mapBorders_[4];
    std::unique_ptr<Line2DObject> mapInnerBorders_[4];
    std::vector<std::unique_ptr<Line2DObject>> gridLines_;
    std::unique_ptr<Circle2DObject> startCircle_;
    std::unique_ptr<Circle2DObject> goalCircle_;
    std::unique_ptr<Ring2DObject> goalPulseRing_;
    std::unique_ptr<Circle2DObject> playerIconCircle_;
    std::vector<std::unique_ptr<Line2DObject>> routeLinePool_;

    // ミニリセットボタン用パーツ
    std::unique_ptr<SpriteObject> mapResetBtnSprite_;
    std::unique_ptr<TextObject2D> mapResetBtnTextObj_;
    std::unique_ptr<TextTextureGenerator> mapResetBtnTextGen_;

    // GOサインパネル用パーツ
    std::unique_ptr<SpriteObject> goBtnSprite_;
    std::unique_ptr<TextObject2D> goBtnTextObj_;
    std::unique_ptr<TextTextureGenerator> goBtnTextGen_;
    std::unique_ptr<SpriteObject> resetBtnSprite_;
    std::unique_ptr<TextObject2D> resetBtnTextObj_;
    std::unique_ptr<TextTextureGenerator> resetBtnTextGen_;

    // タイトルテキスト
    std::unique_ptr<TextObject2D> mapTitleTextObj_;
    std::unique_ptr<TextTextureGenerator> mapTitleTextGen_;
};

