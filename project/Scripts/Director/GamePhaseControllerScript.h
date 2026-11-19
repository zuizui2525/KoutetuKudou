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
    void Draw() override;
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
    bool IsAreaClearing() const { return isAreaClearing_; }
    const Vector3& GetAreaClearCameraPosition() const { return areaClearCurrentCamPos_; }
    const Vector3& GetAreaClearCameraRotation() const { return areaClearCurrentCamRot_; }
    const Vector3& GetDrawCameraPosition() const { return drawCamCurrentPos_; }
    const Vector3& GetDrawCameraRotation() const { return drawCamCurrentRot_; }

    /// <summary>
    /// シーン上でアクティブな GamePhaseControllerScript のインスタンスを取得します。
    /// </summary>
    static GamePhaseControllerScript* GetInstance() { return sInstance_; }

    float GetPlayerMoveSpeed() const { return playerMoveSpeed_; }
    void SetPlayerMoveSpeed(float speed) { playerMoveSpeed_ = speed; }

    /// <summary>
    /// 拘束停止円トラップ発動時に呼び出し、指定秒数あいだ自機の自動走行を強制停止します。
    /// </summary>
    /// <param name="durationSeconds">拘束停止時間 (秒)</param>
    /// <param name="isRushActive">拘束連動エネミーラッシュが発生する場合は true</param>
    void TriggerRestraint(float durationSeconds, bool isRushActive = false);

    /// <summary>
    /// 現在の拘束停止状態を強制解除します。
    /// </summary>
    void ClearRestraint();

    bool IsRestrained() const { return isRestrained_; }
    float GetRestraintRemainingSeconds() const;

    /// <summary>
    /// 現在レベルアップ時のダダサバイバー風3択アップグレード選択中（戦闘ポーズ中）であるかを取得します。
    /// </summary>
    bool IsLevelUpChoosing() const { return isLevelUpChoosing_; }

    /// <summary>
    /// 霊魂吸収時・レベルアップ時などの爽快なHUDポップアップ通知をトリガーします。
    /// </summary>
    void TriggerHudPopup(const std::string& text, const Vector4& color);

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

    /// <summary>
    /// 走行戦闘中に画面上部中央へ水平スクロール式方位バー（自機進行方向基準のコンパスHUD）を描画します。
    /// </summary>
    void RenderCompassDirectX();

    /// <summary>
    /// エリア走破時の「AREA CLEAR!!」祝砲演出・バナー・ゴールドフラッシュをDirectX 12で描画します。
    /// </summary>
    void RenderAreaClearDirectX();

    /// <summary>
    /// 拘束停止円トラップ発動中のHUDカウントダウン警告バナーおよび画面枠パルスを描画します。
    /// </summary>
    void RenderRestraintWarningDirectX();

    /// <summary>
    /// 2Dミニマップの右側に配置するギミック一覧（LEGEND）パネルおよびカメラ切替UIを描画します。
    /// </summary>
    void RenderMapLegendDirectX(float mapPanelMaxX, float mapPanelMaxY, float targetMouseX, float targetMouseY);

    /// <summary>
    /// ルート手書きフェーズ（DrawRoute）中の3Dマップ自動パノラマ展望カメラを更新します。
    /// </summary>
    /// <param name="pauseOrbit">一時停止する場合は true</param>
    void UpdateDrawRouteCamera(bool pauseOrbit);

    /// <summary>
    /// ルート手書きフェーズ（DrawRoute）中の自由操作カメラ（ドラッグ回転・WASD移動・Q/E＆ホイール上下）を更新します。
    /// </summary>
    /// <param name="allowDragRotate">3Dビュー上での左ドラッグ回転を許可する場合は true</param>
    void UpdateFreeRoamCamera(bool allowDragRotate);

    /// <summary>
    /// DrawRouteフェーズのカメラモード（自動見回し ⇔ 自由操作）をシームレスに切り替えます。
    /// </summary>
    void ToggleDrawCameraMode();

    /// <summary>
    /// Step 1-6: レベルアップ3択アップグレード画面を開き、戦闘を一時停止します。
    /// </summary>
    void OpenLevelUpChoice();

    /// <summary>
    /// Step 1-6: 3択カード選択中のマウスホバー・左クリック・1/2/3キー入力を処理します。
    /// </summary>
    void UpdateLevelUpChoice();

    /// <summary>
    /// Step 1-6: 指定スロットの強化を適用し、祝福演出とともに戦闘へ復帰します。
    /// </summary>
    void ConfirmLevelUpChoice(int choiceIndex);

    /// <summary>
    /// Step 1-6: 最上部フルワイドEXPバー、上部左のLV＆所持バフアイコンバー、上部右のステータス要約を描画します。
    /// </summary>
    void RenderExpAndBuffBarDirectX();

    /// <summary>
    /// Step 1-6: ダダサバイバー風の3択アップグレードカード選択画面を描画します。
    /// </summary>
    void RenderLevelUpChoiceDirectX();

    /// <summary>
    /// Step 1-6: 霊魂吸収・パワーアップ時の上昇フェードHUDポップアップを描画します。
    /// </summary>
    void RenderHudPopupDirectX();

    /// <summary>
    /// Step 1-6: 全8種の強化に対応するDX12幾何学ベクターアイコンを任意の中心・サイズで描画します。
    /// </summary>
    void DrawUpgradeIconDirectX(int upgradeTypeIdx, float centerX, float centerY, float iconSize, const Vector4& themeColor);

private:
    // マジックナンバー排除のための定数
    static constexpr int kMaxAreas = 4;
    static constexpr float kPlayerGroundYOffset = 0.72f;    // 自機接地高さオフセット (cube.obj 半径1.0 × スケール0.72m、足元をY=0に接地)
    static constexpr float kDefaultPlayerMoveSpeed = 0.15f; // 毎フレームの標準走行速度（適正値）
    static constexpr float kMinPlayerMoveSpeed = 0.01f;     // 最小速度
    static constexpr float kMaxPlayerMoveSpeed = 2.0f;      // 最大速度
    static constexpr float kDrawCameraHeight = 65.0f;     // 斜め立体俯瞰のカメラ高さ
    static constexpr float kDrawCameraZOffset = -18.0f;   // スタート地点手前のオフセット
    static constexpr float kDrawCameraPitch = 0.98f;      // 約56度の斜め見下ろし角度
    static inline const std::string kClearSceneName = "Clear";
    static inline const std::string kGameOverSceneName = "GameOver";
    static inline const std::string kReticleObjectName = "ReticleUI";

    // エリアクリア演出定数 (マジックナンバー排除)
    static constexpr int kAreaClearDuration = 245;           // エリアクリア演出総フレーム数 (約4.1秒)
    static constexpr int kCameraPullBackDuration = 55;       // 第1幕: カメラ背後引き・回り込みフレーム数 (約0.9秒)
    static constexpr int kPlayerRocketStartFrame = 55;       // 第2幕: 自機打ち上げ開始フレーム
    static constexpr int kPlayerRocketDuration = 65;         // 第2幕: 自機打ち上げ所要フレーム数 (約1.1秒)
    static constexpr float kPlayerFlyForwardDist = 16.0f;    // 上空打ち上げ時の前方前進距離 (m)
    static constexpr float kPlayerFlyMaxHeight = 28.0f;      // 上空打ち上げ到達高度 (m)
    static constexpr float kSendoffCamBackDist = 5.2f;       // 自機後方へのカメラ配置距離 (m)
    static constexpr float kSendoffCamHeight = 1.1f;         // 見送りカメラの基準高さ (m)（低めからダイナミックに見上げる）
    static constexpr float kSendoffCamSideOffset = 0.6f;     // わずかに右肩越しに自機を捉える横オフセット (m)
    static constexpr int kTextDropStartFrame = 125;          // 第3幕: 「AREA CLEAR」文字降下開始フレーム (自機消滅直後)
    static constexpr int kTextDropDuration = 35;             // 第3幕: 文字降下・バウンド所要フレーム数
    static constexpr int kFadeOutStartFrame = 200;           // 第4幕: 黒フェードアウト開始フレーム
    static constexpr float kBlackFadeSpeed = 0.025f;         // 暗転フェード速度 (/frame)
    static constexpr float kAreaClearBannerDropStartY = -180.0f; // 文字降下開始Y座標 (画面外上部)
    static constexpr float kAreaClearBannerTargetY = 0.38f;  // 文字着地目標Y座標比率 (画面高さの38%)
    static constexpr float kAreaClearSlowSpeedRatio = 0.20f; // スローモーション走行速度比率
    static constexpr float kAreaClearFlashFadeSpeed = 2.0f;  // ゴールドフラッシュ減衰速度 (/秒)
    static constexpr float kAreaClearBannerWidth = 620.0f;   // エリアクリアバナー幅 (px)
    static constexpr float kAreaClearBannerHeight = 130.0f;  // エリアクリアバナー高さ (px)

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

    // 画面上部コンパスHUD（方位バー）の定数 (マジックナンバー排除)
    static constexpr float kCompassBarWidth = 380.0f;       // コンパスバーの横幅 (px)
    static constexpr float kCompassBarHeight = 26.0f;      // コンパスバーの高さ (px)
    static constexpr float kCompassBarY = 20.0f;           // 画面上部からのY位置 (px)
    static constexpr float kCompassFovDegrees = 120.0f;     // コンパスバーがカバーする視野角 (度: 左右各60度)
    static constexpr float kCompassTickMajorHeight = 10.0f; // 大目盛りの長さ (px)
    static constexpr float kCompassTickMinorHeight = 6.0f;  // 小目盛りの長さ (px)
    static constexpr int kCompassTickPoolSize = 20;         // 目盛り線オブジェクトのプール数

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

    // 建物突入時の視界悪化・継続ダメージ定数 (マジックナンバー排除)
    static constexpr float kBuildingBlindMaxAlpha = 0.88f;       // 視界悪化の最大アルファ (煙・粉塵でほぼ前方が見えなくなる)
    static constexpr float kBuildingBlindFadeInSpeed = 0.12f;     // スモーク侵入フェードイン速度
    static constexpr float kBuildingBlindFadeOutSpeed = 0.06f;    // スモーク脱出フェードアウト速度
    static constexpr int kBuildingDamageInterval = 18;            // 建物突入時のスリップダメージ間隔 (フレーム数: 約0.3秒ごと)
    static constexpr int kBuildingDamageAmount = 1;               // 建物突入時の被ダメージ量
    static constexpr float kBuildingCollisionShakeIntensity = 0.08f; // 激突カメラシェイク強度
    static constexpr float kBuildingCollisionShakeDuration = 0.15f;  // 激突カメラシェイク持続時間

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
    std::vector<std::unique_ptr<Circle2DObject>> mapEnemyCircles_;
    std::vector<std::unique_ptr<Ring2DObject>> mapEnemyRings_;
    std::vector<std::unique_ptr<SpriteObject>> mapBuildingSprites_;

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

    // 画面上部コンパスHUD（方位バー）パーツ (DirectX 12)
    bool isCompassEnabled_ = true;
    std::unique_ptr<SpriteObject> compassBgSprite_;
    std::unique_ptr<Line2DObject> compassBorders_[4];
    std::unique_ptr<Line2DObject> compassCenterMarker_;
    std::vector<std::unique_ptr<Line2DObject>> compassTickLines_;
    struct CompassLabelItem {
        float angleDeg = 0.0f;
        std::string text;
        Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::unique_ptr<TextObject2D> textObj;
        std::unique_ptr<TextTextureGenerator> textGen;
        float textWidth = 0.0f;
        float textHeight = 0.0f;
    };
    std::vector<CompassLabelItem> compassLabels_;

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

    // 建物突入時の視界悪化スモーク＆継続スリップダメージ用パーツ (DirectX 12)
    std::unique_ptr<SpriteObject> buildingBlindSprite_;
    float buildingBlindAlpha_ = 0.0f;
    int buildingDamageTimer_ = 0;

    // エリアクリア祝砲演出用パーツ (DirectX 12)
    bool isAreaClearing_ = false;
    int areaClearTimer_ = 0;
    float areaClearFlashAlpha_ = 0.0f;
    Vector3 goalPlayerPos_{};
    Vector3 goalPlayerDir_{};
    Vector3 sendoffCamPos_{};
    Vector3 areaClearInitialCamPos_{};   // ゴール到達時の一人称カメラ座標 (第1幕で後方引き補間用)
    Vector3 areaClearCurrentCamPos_{};   // エリアクリア演出中の現在カメラ座標 (FPSCameraControllerScript同期用)
    Vector3 areaClearCurrentCamRot_{};   // エリアクリア演出中の現在カメラ回転 (FPSCameraControllerScript同期用)
    std::unique_ptr<SpriteObject> areaClearBannerSprite_;
    std::unique_ptr<TextObject2D> areaClearTextObj_;
    std::unique_ptr<TextTextureGenerator> areaClearTextGen_;
    std::unique_ptr<TextObject2D> areaClearSubTextObj_;
    std::unique_ptr<TextTextureGenerator> areaClearSubTextGen_;

    // シームレス暗転フェード用パーツ (DirectX 12: 準備前描画の完全防止)
    float transitionBlackFadeAlpha_ = 0.0f;
    float drawPhaseFadeAlpha_ = 0.0f;
    std::unique_ptr<SpriteObject> transitionBlackSprite_;

    // 拘束停止円トラップ（Restraint Circle）制御・HUDカウントダウン警告用パーツ (DirectX 12)
    static constexpr float kRestraintTargetFps = 60.0f;
    static constexpr float kRestraintLockShakeIntensity = 0.22f;
    static constexpr float kRestraintLockShakeDuration = 0.25f;
    static constexpr float kRestraintPanelWidth = 360.0f;
    static constexpr float kRestraintPanelHeight = 76.0f;
    static constexpr float kRestraintPanelTopMargin = 92.0f;
    static constexpr float kRestraintBarHeight = 10.0f;
    static constexpr float kRestraintBarPadding = 16.0f;
    static constexpr float kRestraintPulseSpeed = 0.14f;
    static constexpr float kRestraintEdgeThickness = 5.0f;
    static constexpr float kRestraintTitleFontSize = 16.0f;
    static constexpr float kRestraintTimerFontSize = 22.0f;

    bool isRestrained_ = false;
    bool isRestraintRushActive_ = false;
    int restraintTimerFrames_ = 0;
    int restraintDurationFrames_ = 0;
    float restraintPulseTimer_ = 0.0f;
    std::unique_ptr<SpriteObject> restraintPanelBgSprite_;
    std::unique_ptr<Line2DObject> restraintPanelBorders_[4];
    std::unique_ptr<SpriteObject> restraintBarBgSprite_;
    std::unique_ptr<SpriteObject> restraintBarFillSprite_;
    std::unique_ptr<Line2DObject> restraintScreenEdges_[4];
    std::unique_ptr<TextObject2D> restraintTitleTextObj_;
    std::unique_ptr<TextTextureGenerator> restraintTitleTextGen_;
    std::unique_ptr<TextTextureGenerator> restraintRushTitleTextGen_;
    std::unique_ptr<TextObject2D> restraintTimerTextObj_;
    std::unique_ptr<TextTextureGenerator> restraintTimerTextGen_;
    std::string lastRenderedRestraintTimerText_;
    float cachedRestraintTitleWidth_ = 0.0f;
    float cachedRestraintTitleHeight_ = 0.0f;
    float cachedRestraintRushTitleWidth_ = 0.0f;
    float cachedRestraintRushTitleHeight_ = 0.0f;
    float cachedRestraintTimerWidth_ = 0.0f;
    float cachedRestraintTimerHeight_ = 0.0f;
    std::vector<std::unique_ptr<Circle2DObject>> mapEnemyCoreDots_;
    std::vector<std::unique_ptr<Line2DObject>> mapBossDiamondLines_;
    std::vector<std::unique_ptr<Circle2DObject>> mapTrapCircles_;
    std::vector<std::unique_ptr<Ring2DObject>> mapTrapRings_;
    std::vector<std::unique_ptr<Ring2DObject>> mapTrapInnerRings_;

    // 2Dミニマップ円内幾何学アイコン定数（文字重ね廃止・形と色で直感識別）
    static constexpr float kMapEnemyCoreDotRadius = 2.8f;     // 通常敵エリア中心ドット半径 (px)
    static constexpr float kMapBossDiamondHalfSize = 5.5f;    // ボスエリア中心ひし形(◆)半サイズ (px)
    static constexpr float kMapTrapInnerRingRatio = 0.62f;    // 拘束トラップ内側リング半径比率

    // 画面右下 ギミック一覧（GIMMICK LEGEND）パネル用パーツ (DirectX 12: 中央GOボタンと重ならない左右対称配置)
    static constexpr int kLegendItemCount = 5;
    static constexpr float kLegendPanelWidth = 210.0f;
    static constexpr float kLegendPanelHeight = 168.0f;
    static constexpr float kLegendMarginRight = 32.0f;
    static constexpr float kLegendPadding = 12.0f;
    static constexpr float kLegendHeaderHeight = 28.0f;
    static constexpr float kLegendRowHeight = 25.0f;
    static constexpr float kLegendIconRadius = 6.5f;
    static constexpr float kLegendHeaderFontSize = 13.0f;
    static constexpr float kLegendLabelFontSize = 13.0f;
    std::unique_ptr<SpriteObject> legendBgSprite_;
    std::unique_ptr<Line2DObject> legendBorders_[4];
    std::unique_ptr<TextObject2D> legendHeaderTextObj_;
    std::unique_ptr<TextTextureGenerator> legendHeaderTextGen_;
    std::unique_ptr<Circle2DObject> legendIconCircles_[4];
    std::unique_ptr<Circle2DObject> legendEnemyCoreDot_;
    std::unique_ptr<Ring2DObject> legendIconRings_[2];
    std::unique_ptr<Ring2DObject> legendTrapInnerRing_;
    std::unique_ptr<SpriteObject> legendBuildingIconSprite_;
    std::unique_ptr<TextObject2D> legendItemTextObjs_[kLegendItemCount];
    std::unique_ptr<TextTextureGenerator> legendItemTextGens_[kLegendItemCount];
    float legendItemTextHeights_[kLegendItemCount]{};

    // カメラモード切替ボタン＆自由カメラ操作ヒント用パーツ (DirectX 12)
    static constexpr float kCamToggleBtnHeight = 34.0f;
    static constexpr float kCamToggleBtnGap = 8.0f;
    static constexpr float kCamToggleFontSize = 13.0f;
    static constexpr float kFreeCamHintHeight = 42.0f;
    static constexpr float kFreeCamHintFontSize = 11.0f;
    std::unique_ptr<SpriteObject> camToggleBtnSprite_;
    std::unique_ptr<Line2DObject> camToggleBtnBorders_[4];
    std::unique_ptr<TextObject2D> camToggleTextObj_;
    std::unique_ptr<TextTextureGenerator> camToggleTextAutoGen_;
    std::unique_ptr<TextTextureGenerator> camToggleTextFreeGen_;
    float camToggleAutoW_ = 0.0f;
    float camToggleAutoH_ = 0.0f;
    float camToggleFreeW_ = 0.0f;
    float camToggleFreeH_ = 0.0f;
    std::unique_ptr<SpriteObject> freeCamHintBgSprite_;
    std::unique_ptr<Line2DObject> freeCamHintBorders_[4];
    std::unique_ptr<TextObject2D> freeCamHintLine1Obj_;
    std::unique_ptr<TextTextureGenerator> freeCamHintLine1Gen_;
    std::unique_ptr<TextObject2D> freeCamHintLine2Obj_;
    std::unique_ptr<TextTextureGenerator> freeCamHintLine2Gen_;

    // DrawRouteフェーズ 3Dマップカメラ（自動見回し ⇔ 自由操作）定数＆状態
    enum class DrawCameraMode {
        AutoOrbit, // 自動パノラマ見回しカメラ
        FreeRoam   // 自由操作カメラ（ドラッグ回転・WASD移動・Q/E＆ホイール上下）
    };
    static constexpr float kDrawCamOrbitRadius = 78.0f;       // エリア中央からの水平パノラマ距離 (m)
    static constexpr float kDrawCamBaseHeight = 45.0f;        // 基準高度 (m)
    static constexpr float kDrawCamHeightAmp = 7.0f;          // 高度の上下うねり幅 (m)
    static constexpr float kDrawCamSwingSpeed = 0.008f;       // パノラマ自動見回し速度 (rad/frame)
    static constexpr float kDrawCamMaxSwingAngle = 0.73f;     // 左右最大スウィング角度 (約±42度)
    static constexpr float kDrawCamLookZSlideAmp = 18.0f;     // 注視点の前後スライド幅 (m)
    static constexpr float kDrawCamLookY = 4.0f;              // 注視点の高さ (m)
    static constexpr float kFreeCamMoveSpeed = 0.85f;         // 自由カメラ水平移動速度 (m/frame)
    static constexpr float kFreeCamVerticalSpeed = 0.65f;     // 自由カメラ上下移動速度 (m/frame)
    static constexpr float kFreeCamWheelSpeed = 0.03f;        // マウスホイール高度ズーム係数
    static constexpr float kFreeCamRotateSensitivity = 0.0045f; // 左ドラッグ視点旋回感度 (rad/px)
    static constexpr float kFreeCamMinHeight = 8.0f;          // 自由カメラ最低高度 (m)
    static constexpr float kFreeCamMaxHeight = 95.0f;         // 自由カメラ最高高度 (m)
    static constexpr float kFreeCamMinPitch = 0.05f;          // 自由カメラ最小ピッチ角 (rad)
    static constexpr float kFreeCamMaxPitch = 1.48f;          // 自由カメラ最大見下ろしピッチ角 (rad)
    static constexpr float kFreeCamBoundX = 65.0f;            // 自由カメラ水平X移動限界 (m)
    static constexpr float kFreeCamBoundZMargin = 50.0f;      // 自由カメラエリア前後Z余白 (m)

    DrawCameraMode drawCamMode_ = DrawCameraMode::AutoOrbit;
    float drawCamOrbitTimer_ = 0.0f;
    Vector3 drawCamCurrentPos_{};
    Vector3 drawCamCurrentRot_{};

    // Step 1-6: EXPバー・画面上部所持バフアイコンバー・ダダサバイバー風3択カードUI 定数＆描画オブジェクト
    static constexpr int kMaxBuffIconSlots = 8;
    static constexpr int kLevelUpCardCount = 3;
    static constexpr int kLevelUpInputGuardFrames = 15;       // 表示直後の誤クリック防止ガード (約0.25秒)
    static constexpr int kHudPopupDurationFrames = 95;        // ポップアップ表示持続フレーム数 (約1.6秒)
    static constexpr float kTopExpBarHeight = 12.0f;          // 最上部フルワイドEXPバー高さ (px)
    static constexpr float kLvBadgeX = 20.0f;                 // LVバッジ左端X (px)
    static constexpr float kLvBadgeY = 20.0f;                 // LVバッジ上端Y (px)
    static constexpr float kLvBadgeWidth = 74.0f;             // LVバッジ幅 (px)
    static constexpr float kLvBadgeHeight = 56.0f;            // LVバッジ高さ (px)
    static constexpr float kLvBadgeGaugePadX = 6.0f;          // LVバッジ内EXPゲージ左右余白 (px)
    static constexpr float kLvBadgeGaugeHeight = 8.0f;        // LVバッジ内EXPゲージ高さ (px)
    static constexpr float kLvBadgeGaugeBottomPad = 7.0f;     // LVバッジ内EXPゲージ下余白 (px)
    static constexpr float kBuffBarStartX = 102.0f;           // 所持バフアイコンバー開始X (px)
    static constexpr float kBuffSlotWidth = 38.0f;            // バフアイコン1スロット幅 (px)
    static constexpr float kBuffSlotHeight = 56.0f;           // バフアイコン1スロット高さ (px)
    static constexpr float kBuffSlotGap = 5.0f;               // バフアイコン間隔 (px)
    static constexpr float kStatsPanelWidth = 320.0f;         // 右上ステータス要約パネル幅 (px)
    static constexpr float kStatsPanelHeight = 36.0f;         // 右上ステータス要約パネル高さ (px)
    static constexpr float kStatsPanelMarginRight = 20.0f;    // 右上パネル右マージン (px)
    static constexpr float kLevelUpHeaderWidth = 580.0f;      // LEVEL UP見出しパネル幅 (px)
    static constexpr float kLevelUpHeaderHeight = 88.0f;      // LEVEL UP見出しパネル高さ (px)
    static constexpr float kLevelUpHeaderY = 86.0f;           // LEVEL UP見出し上端Y (px)
    static constexpr float kChoiceCardWidth = 310.0f;         // 3択カード幅 (px)
    static constexpr float kChoiceCardHeight = 440.0f;        // 3択カード高さ (px)
    static constexpr float kChoiceCardGap = 28.0f;            // 3択カード間隔 (px)
    static constexpr float kChoiceCardBaseY = 196.0f;         // 3択カード基準上端Y (px)
    static constexpr float kChoiceCardHoverLiftY = 10.0f;     // ホバー時のカード浮き上がり量 (px)
    static constexpr float kChoiceCardBandHeight = 36.0f;     // カード上部カテゴリ帯高さ (px)
    static constexpr float kChoiceCardIconBoxSize = 84.0f;    // カード内大型アイコン枠サイズ (px)
    static constexpr float kChoiceCardBtnHeight = 40.0f;      // カード下部選択ボタン高さ (px)
    static constexpr float kHudPopupBaseY = 158.0f;           // ポップアップ開始Y座標 (px)
    static constexpr float kHudPopupRiseDist = 34.0f;         // ポップアップ上昇距離 (px)

    static constexpr int kIconLinePoolSize = 96;
    static constexpr int kIconCirclePoolSize = 28;
    static constexpr int kIconRingPoolSize = 28;
    static constexpr int kIconSpritePoolSize = 40;

    bool isLevelUpChoosing_ = false;
    int levelUpInputGuardTimer_ = 0;
    int hoveredCardIndex_ = -1;
    float levelUpAnimTimer_ = 0.0f;
    std::vector<int> currentUpgradeChoices_;                  // 現在の3択 (PlayerControllerScript::UpgradeType の int 値)

    // HUD ポップアップ状態
    std::string hudPopupText_;
    Vector4 hudPopupColor_ = { 0.35f, 0.98f, 1.0f, 1.0f };
    int hudPopupTimer_ = 0;

    // 1. 最上部EXPバー ＆ LVバッジ ＆ 所持バフアイコンバー ＆ 右上ステータス要約パーツ
    std::unique_ptr<SpriteObject> expBarBgSprite_;
    std::unique_ptr<SpriteObject> expBarFillSprite_;
    std::unique_ptr<Line2DObject> expBarBottomLine_;

    std::unique_ptr<SpriteObject> lvBadgeBgSprite_;
    std::unique_ptr<SpriteObject> lvExpBarBgSprite_;
    std::unique_ptr<SpriteObject> lvExpBarFillSprite_;
    std::unique_ptr<Line2DObject> lvBadgeBorders_[4];
    std::unique_ptr<TextObject2D> lvBadgeTextObj_;
    std::unique_ptr<TextTextureGenerator> lvBadgeTextGen_;

    std::unique_ptr<SpriteObject> buffSlotBgSprites_[kMaxBuffIconSlots];
    std::unique_ptr<Line2DObject> buffSlotBorders_[kMaxBuffIconSlots][4];
    std::unique_ptr<TextObject2D> buffSlotLvTextObjs_[kMaxBuffIconSlots];
    std::unique_ptr<TextTextureGenerator> buffSlotLvTextGens_[kMaxBuffIconSlots];

    std::unique_ptr<SpriteObject> statsPanelBgSprite_;
    std::unique_ptr<Line2DObject> statsPanelBorders_[4];
    std::unique_ptr<TextObject2D> statsPanelTextObj_;
    std::unique_ptr<TextTextureGenerator> statsPanelTextGen_;

    // 2. 3択アップグレード選択画面パーツ
    std::unique_ptr<SpriteObject> levelUpDimSprite_;
    std::unique_ptr<SpriteObject> levelUpHeaderBgSprite_;
    std::unique_ptr<Line2DObject> levelUpHeaderBorders_[4];
    std::unique_ptr<TextObject2D> levelUpHeaderTextObj_;
    std::unique_ptr<TextTextureGenerator> levelUpHeaderTextGen_;
    std::unique_ptr<TextObject2D> levelUpSubTextObj_;
    std::unique_ptr<TextTextureGenerator> levelUpSubTextGen_;

    std::unique_ptr<SpriteObject> choiceCardBgSprites_[kLevelUpCardCount];
    std::unique_ptr<SpriteObject> choiceCardTopBandSprites_[kLevelUpCardCount];
    std::unique_ptr<SpriteObject> choiceCardIconBoxSprites_[kLevelUpCardCount];
    std::unique_ptr<SpriteObject> choiceCardBtnSprites_[kLevelUpCardCount];
    std::unique_ptr<Line2DObject> choiceCardBorders_[kLevelUpCardCount][4];
    std::unique_ptr<Line2DObject> choiceCardIconBoxBorders_[kLevelUpCardCount][4];
    std::unique_ptr<TextObject2D> choiceCardCatTextObjs_[kLevelUpCardCount];
    std::unique_ptr<TextTextureGenerator> choiceCardCatTextGens_[kLevelUpCardCount];
    std::unique_ptr<TextObject2D> choiceCardTitleTextObjs_[kLevelUpCardCount];
    std::unique_ptr<TextTextureGenerator> choiceCardTitleTextGens_[kLevelUpCardCount];
    std::unique_ptr<TextObject2D> choiceCardLvTextObjs_[kLevelUpCardCount];
    std::unique_ptr<TextTextureGenerator> choiceCardLvTextGens_[kLevelUpCardCount];
    std::unique_ptr<TextObject2D> choiceCardDesc1TextObjs_[kLevelUpCardCount];
    std::unique_ptr<TextTextureGenerator> choiceCardDesc1TextGens_[kLevelUpCardCount];
    std::unique_ptr<TextObject2D> choiceCardDesc2TextObjs_[kLevelUpCardCount];
    std::unique_ptr<TextTextureGenerator> choiceCardDesc2TextGens_[kLevelUpCardCount];
    std::unique_ptr<TextObject2D> choiceCardKeyTextObjs_[kLevelUpCardCount];
    std::unique_ptr<TextTextureGenerator> choiceCardKeyTextGens_[kLevelUpCardCount];

    // 3. 共通ベクターアイコン描画プール (フレーム内GPUバッファ上書き防止)
    std::vector<std::unique_ptr<Line2DObject>> iconLinePool_;
    std::vector<std::unique_ptr<Circle2DObject>> iconCirclePool_;
    std::vector<std::unique_ptr<Ring2DObject>> iconRingPool_;
    std::vector<std::unique_ptr<SpriteObject>> iconSpritePool_;
    size_t iconLineCursor_ = 0;
    size_t iconCircleCursor_ = 0;
    size_t iconRingCursor_ = 0;
    size_t iconSpriteCursor_ = 0;

    // 4. HUD ポップアップパーツ
    std::unique_ptr<SpriteObject> hudPopupBgSprite_;
    std::unique_ptr<TextObject2D> hudPopupTextObj_;
    std::unique_ptr<TextTextureGenerator> hudPopupTextGen_;

    // シングルトンインスタンス参照
    static inline GamePhaseControllerScript* sInstance_ = nullptr;
};
