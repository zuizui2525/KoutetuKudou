#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include "App/Scene/Core/SceneTransitionConfig.h"

class PostProcess;
struct ImDrawList;

/// <summary>
/// シーン遷移の状態（ステートマシン）
/// </summary>
enum class TransitionPhase {
    Idle,           // 待機中（通常ゲーム/エディタ動作中）
    FadeOut,        // 前半: 暗転・ポストエフェクト進行
    Loading,        // 中間: 完全暗転維持 & ロード実行 (Now Loading)
    FadeIn,         // 後半: 明転・ポストエフェクト復帰
    PreviewFadeOut, // プレビュー用前半（シーン切替なし）
    PreviewFadeIn,  // プレビュー用後半
};

/// <summary>
/// PowerPointライクな画面切り替えとポストプロセス連動を統括するマネージャー
/// </summary>
class SceneTransitionManager {
public:
    static SceneTransitionManager* GetInstance();

    /// <summary>
    /// 初期化（デフォルト設定のロードなど）
    /// </summary>
    void Initialize();

    /// <summary>
    /// 毎フレームの更新処理（タイマー進行、進行度計算、PostProcess補間）
    /// </summary>
    /// <param name="deltaTime">経過時間（秒）</param>
    void Update(float deltaTime);

#ifdef _USEIMGUI
    /// <summary>
    /// エディタ Game View 領域への最前面オーバーレイ描画（ImGui DrawList 連携）
    /// </summary>
    /// <param name="drawList">ImGuiの描画リスト</param>
    /// <param name="posMin">描画矩形の左上座標</param>
    /// <param name="posMax">描画矩形の右下座標</param>
    void DrawEditorOverlay(ImDrawList* drawList, const Vector2& posMin, const Vector2& posMax);
#endif

    /// <summary>
    /// 指定シーンへの画面切り替えを開始（退場元OUT演出 -> 入場先IN演出）
    /// </summary>
    /// <param name="fromSceneName">現在の退場元シーン名</param>
    /// <param name="toSceneName">遷移先の入場先シーン名</param>
    void StartTransition(const std::string& fromSceneName, const std::string& toSceneName);

    /// <summary>
    /// OUT（退場演出）のみをテスト再生
    /// </summary>
    void TriggerPreviewOut(const SingleTransitionSetting& setting);

    /// <summary>
    /// IN（入場演出）のみをテスト再生
    /// </summary>
    void TriggerPreviewIn(const SingleTransitionSetting& setting);

    /// <summary>
    /// 通しプレビュー（OUT演出 -> IN演出）をテスト再生
    /// </summary>
    void TriggerPreviewFull(const SingleTransitionSetting& outSetting, const SingleTransitionSetting& inSetting);

    /// <summary>
    /// 現在遷移アニメーション中（またはプレビュー中）かどうか
    /// </summary>
    bool IsTransitioning() const;

    /// <summary>
    /// 現在ロードフェーズ（Now Loading表示中）かどうか
    /// </summary>
    bool IsLoading() const { return currentPhase_ == TransitionPhase::Loading; }

    /// <summary>
    /// 現在のフェーズを取得
    /// </summary>
    TransitionPhase GetCurrentPhase() const { return currentPhase_; }

    /// <summary>
    /// 現在のアニメーション進行度 (0.0f ～ 1.0f)
    /// </summary>
    float GetCurrentProgress() const { return currentProgress_; }

    /// <summary>
    /// シーン別の設定を取得（未設定ならデフォルト値を返す）
    /// </summary>
    SceneTransitionConfig GetConfig(const std::string& sceneName);

    /// <summary>
    /// シーン別の設定を登録・更新
    /// </summary>
    void SetConfig(const std::string& sceneName, const SceneTransitionConfig& config);

    /// <summary>
    /// シーン設定をJSONファイルへ保存
    /// </summary>
    void SaveConfigToFile(const std::string& sceneName);

    /// <summary>
    /// シーン設定をJSONファイルから読み込み
    /// </summary>
    void LoadConfigFromFile(const std::string& sceneName);

    /// <summary>
    /// PostProcessインスタンスの参照を設定
    /// </summary>
    void SetPostProcess(PostProcess* postProcess) { postProcess_ = postProcess; }

    /// <summary>
    /// 遷移中に画面が完全に隠れたタイミングで呼ばれるコールバック（SceneManagerが設定）
    /// </summary>
    void SetOnSceneSwitchCallback(std::function<void(const std::string&)> callback) {
        onSceneSwitchCallback_ = callback;
    }

private:
    SceneTransitionManager() = default;
    ~SceneTransitionManager() = default;
    SceneTransitionManager(const SceneTransitionManager&) = delete;
    SceneTransitionManager& operator=(const SceneTransitionManager&) = delete;

    // イージング関数 (Smoothstep / EaseInOut)
    float CalculateEaseInOut(float t) const;

    // PostProcessパラメータの更新適用
    void ApplyPostEffects(float progress, const SingleTransitionSetting& setting, bool isFadeOut);

    // ポストプロセスのエフェクト解除
    void ResetPostEffects();

private:
    TransitionPhase currentPhase_ = TransitionPhase::Idle;
    SingleTransitionSetting activeOutSetting_{};
    SingleTransitionSetting activeInSetting_{};
    SingleTransitionSetting currentActiveSetting_{};
    std::string pendingSceneName_ = "";

    float timer_ = 0.0f;
    float currentProgress_ = 0.0f; // 0.0f ～ 1.0f

    PostProcess* postProcess_ = nullptr;
    std::function<void(const std::string&)> onSceneSwitchCallback_ = nullptr;

    // シーン名ごとの設定キャッシュ
    std::unordered_map<std::string, SceneTransitionConfig> sceneConfigs_;

    // マジックナンバー排除用の定数
    static constexpr float kMinProgress = 0.0f;
    static constexpr float kMaxProgress = 1.0f;
    static constexpr float kHalfValue = 0.5f;
    static constexpr float kZeroTime = 0.0f;
    static constexpr float kDefaultLoadingDuration = 0.5f; // 完全暗転（ロード待機）の最低維持秒数
};
