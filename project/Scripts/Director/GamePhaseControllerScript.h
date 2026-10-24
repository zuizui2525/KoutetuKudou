#pragma once
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Audio/AudioStructs.h"
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
        Countdown, // 発進前カウントダウン演出フェーズ
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

    /// <summary>
    /// ルート手書きフェーズ（DrawRoute）へ遷移します。
    /// カメラを見下ろし視点へ切り替え、照準UIを非表示にしてルート入力を待機します。
    /// </summary>
    void TransitionToDraw();

    /// <summary>
    /// カウントダウン演出フェーズ（Countdown）へ遷移します。
    /// 自機をスタート位置に配置し、カメラを走行視点へ切り替え、3秒カウントダウンを開始します。
    /// </summary>
    void TransitionToCountdown();

    /// <summary>
    /// 走行・戦闘フェーズ（Play）へ遷移します。
    /// カメラをFPS自機視点へ同期し、照準UIを表示して自動等速前進を開始します。
    /// </summary>
    void TransitionToPlay();

    Phase GetCurrentPhase() const { return currentPhase_; }
    int GetCurrentArea() const { return currentArea_; }

    float GetPlayerMoveSpeed() const { return playerMoveSpeed_; }
    void SetPlayerMoveSpeed(float speed) { playerMoveSpeed_ = speed; }

private:
    /// <summary>
    /// ルート手書きフェーズ中の入力（マウスドラッグ、ミニマップ内クリック、GO/リセットボタン判定）を処理します。
    /// </summary>
    void UpdateDrawRoutePhase();

    /// <summary>
    /// 発進前カウントダウン演出フェーズ（3, 2, 1, START!!）のタイマー更新を処理します。
    /// </summary>
    void UpdateCountdownPhase();

    /// <summary>
    /// 走行・戦闘フェーズ中の自機等速前進、向き・回転の反映、およびゴール到達判定を処理します。
    /// </summary>
    void UpdatePlayPhase();

    /// <summary>
    /// 現在のエリアのゴールに到達した際、全4エリアの走破状況を確認し、
    /// 最終エリアであればクリアシーンへ、そうでなければ次エリアのDrawRouteフェーズへ進めます。
    /// </summary>
    void HandleAreaClear();

    void DrawHUD();
    void Draw2DMap(const Vector2& viewPosMin, const Vector2& viewSize);
    void DrawGoSign(const Vector2& viewPosMin, const Vector2& viewSize);

    /// <summary>
    /// シーン上の残存するすべての敵キャラクターを安全に非表示・消滅させます。
    /// </summary>
    void ClearAllEnemies();

    /// <summary>
    /// レティクル（照準UI）の表示/非表示状態を切り替えます。
    /// </summary>
    /// <param name="visible">表示する場合 true</param>
    void SetReticleVisible(bool visible);

    // DirectX 12 ベースの 2D オブジェクト描画メソッド
    /// <summary>
    /// 2Dマップパネル、GOサイン、ボタン用のDirectX 12スプライト・ライン・テキスト描画オブジェクトを初期化します。
    /// </summary>
    void Init2DObjects();

    /// <summary>
    /// 画面左下に配置されたミニマップパネル、スタート/ゴールサークル、自機アイコン、ルート折れ線をDirectX 12で描画します。
    /// </summary>
    void Render2DMapDirectX();

    /// <summary>
    /// ルート完成時に画面中央下に表示されるGOボタンおよびリセットボタンをDirectX 12で描画します。
    /// </summary>
    void RenderGoSignDirectX();

    /// <summary>
    /// ルート手書きフェーズ中に画面上部に案内ガイダンス（「線を引いてください」等）をDirectX 12で描画します。
    /// </summary>
    void RenderGuidanceDirectX();

    /// <summary>
    /// 発進前カウントダウンフェーズ中に画面中央に大型カウントダウン数字をDirectX 12で描画します。
    /// </summary>
    void RenderCountdownDirectX();

    /// <summary>
    /// 走行・戦闘フェーズ（Play）中に画面端に自機HPバー、残弾数、リロード状態、エリア進捗をDirectX 12で描画します。
    /// </summary>
    void RenderPlayHUDDirectX();

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

    // 走行戦闘中HUDの定数（DirectX 12）
    static constexpr float kHudHpPanelWidth = 260.0f;     // 自機HPパネル幅 (px)
    static constexpr float kHudHpPanelHeight = 44.0f;    // 自機HPパネル高さ (px)
    static constexpr float kHudHpMarginLeft = 32.0f;     // 左マージン (px)
    static constexpr float kHudHpMarginBottom = 32.0f;   // 下マージン (px)
    static constexpr float kHudHpBarPadding = 6.0f;      // HPゲージ内側余白 (px)
    static constexpr float kHudHpBarHeight = 12.0f;      // HPゲージ高さ (px)

    static constexpr float kHudAmmoPanelWidth = 210.0f;  // 残弾数パネル幅 (px)
    static constexpr float kHudAmmoPanelHeight = 44.0f;  // 残弾数パネル高さ (px)
    static constexpr float kHudAmmoMarginRight = 32.0f;  // 右マージン (px)
    static constexpr float kHudAmmoMarginBottom = 32.0f; // 下マージン (px)

    static constexpr float kHudAreaPanelWidth = 240.0f;  // エリア進行度パネル幅 (px)
    static constexpr float kHudAreaPanelHeight = 36.0f;  // エリア進行度パネル高さ (px)
    static constexpr float kHudAreaMarginTop = 24.0f;    // 上マージン (px)

    // ガイダンスパネル定数
    static constexpr float kGuidancePanelWidth = 560.0f;  // ガイダンスパネル幅 (px)
    static constexpr float kGuidancePanelHeight = 44.0f;  // ガイダンスパネル高さ (px)
    static constexpr float kGuidancePanelMarginTop = 28.0f; // 上マージン (px)

    // カウントダウン表示定数
    static constexpr float kCountdownDuration = 3.5f;     // カウントダウン総秒数
    static constexpr float kCountdownBoxWidth = 320.0f;   // カウントダウン背景幅 (px)
    static constexpr float kCountdownBoxHeight = 160.0f;  // カウントダウン背景高さ (px)

    // サウンド・エフェクト・アニメーション定数
    static inline const std::string kSeCountdownBeepPath = "resources/Sounds/countdown_beep.wav";
    static inline const std::string kSeStartSignalPath = "resources/Sounds/start_signal.wav";
    static inline const std::string kSeEngineBoostPath = "resources/Sounds/engine_boost.wav";

    static inline const std::string kEffectCountdownAura = "RingAura";
    static inline const std::string kEffectStartRing = "RingSlash";
    static inline const std::string kEffectStartSparkle = "FireworksSparkle";

    static constexpr float kFlashInitialAlpha = 0.85f;    // 発進白フラッシュ最大アルファ
    static constexpr float kFlashFadeSpeed = 2.8f;        // 白フラッシュ減衰速度 (/秒)
    static constexpr float kPulseMaxScale = 1.35f;        // 数字パルス最大拡大率
    static constexpr float kPulseDecayRate = 7.0f;        // パルス減衰速度

    // メンバ変数
    Phase currentPhase_ = Phase::DrawRoute;
    int currentArea_ = 0;
    float currentDistance_ = 0.0f;
    float playerMoveSpeed_ = kDefaultPlayerMoveSpeed;
    bool isSceneChanging_ = false;

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

    // 走行戦闘フェーズ用HUDパーツ (DirectX 12)
    std::unique_ptr<SpriteObject> hudHpBgSprite_;
    std::unique_ptr<SpriteObject> hudHpBarBgSprite_;
    std::unique_ptr<SpriteObject> hudHpBarFillSprite_;
    std::unique_ptr<Line2DObject> hudHpBorders_[4];
    std::unique_ptr<TextObject2D> hudHpTextObj_;
    std::unique_ptr<TextTextureGenerator> hudHpTextGen_;

    std::unique_ptr<SpriteObject> hudAmmoBgSprite_;
    std::unique_ptr<Line2DObject> hudAmmoBorders_[4];
    std::unique_ptr<TextObject2D> hudAmmoTextObj_;
    std::unique_ptr<TextTextureGenerator> hudAmmoTextGen_;

    std::unique_ptr<SpriteObject> hudAreaBgSprite_;
    std::unique_ptr<Line2DObject> hudAreaBorders_[4];
    std::unique_ptr<TextObject2D> hudAreaTextObj_;
    std::unique_ptr<TextTextureGenerator> hudAreaTextGen_;

    // ガイダンスパネル用パーツ (DirectX 12)
    std::unique_ptr<SpriteObject> guidanceBgSprite_;
    std::unique_ptr<Line2DObject> guidanceBorders_[4];
    std::unique_ptr<TextObject2D> guidanceTextObj_;
    std::unique_ptr<TextTextureGenerator> guidanceTextGen_;
    std::string lastGuidanceText_;

    // カウントダウンフェーズ用パーツ (DirectX 12)
    float countdownTimer_ = 0.0f;
    int lastCountdownSec_ = -1;
    std::unique_ptr<SpriteObject> countdownBgSprite_;
    std::unique_ptr<Line2DObject> countdownBorders_[4];
    std::unique_ptr<TextObject2D> countdownTextObj_;
    std::unique_ptr<TextTextureGenerator> countdownTextGen_;
    std::unique_ptr<TextObject2D> countdownSubTextObj_;
    std::unique_ptr<TextTextureGenerator> countdownSubTextGen_;
    std::string lastRenderedCountdownText_;
    std::string lastRenderedCountdownSubText_;
    float cachedCountdownTextWidth_ = 0.0f;
    float cachedCountdownTextHeight_ = 0.0f;
    float cachedCountdownSubWidth_ = 0.0f;
    float cachedCountdownSubHeight_ = 0.0f;

    // 効果音 (SE) データ
    SoundData seCountdownBeep_{};
    SoundData seStartSignal_{};
    SoundData seEngineBoost_{};
    bool isSeLoaded_ = false;

    // パルス＆フラッシュ演出用
    float pulseTimer_ = 0.0f;
    float flashAlpha_ = 0.0f;
    std::unique_ptr<SpriteObject> flashSprite_;
};

