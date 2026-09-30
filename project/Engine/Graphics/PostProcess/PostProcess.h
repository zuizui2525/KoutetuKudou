#pragma once
#include <d3d12.h>
#include <memory>
#include <vector>
#include "Engine/Graphics/Texture/RenderTexture.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/PostProcess/IPostProcessPass.h"

enum class PostClearColorMode : int32_t {
    Blue = 0,  // 元々の青背景
    Red = 1,   // デバッグ赤
    Black = 2  // デバッグ黒
};

/// <summary>
/// ポストプロセス管理クラス（マルチパス・パイプライン）
/// </summary>
class PostProcess {
public:
    PostProcess() = default;
    ~PostProcess() = default;

    /// <summary>
    /// 初期化（中間バッファ作成と全パスの登録・初期化）
    /// </summary>
    void Initialize();

    /// <summary>
    /// 描画前処理（メインのレンダーテクスチャを描画ターゲットに設定してクリア）
    /// </summary>
    void PreDraw();

    /// <summary>
    /// 描画後処理（メインレンダーテクスチャのリソース状態を読み込み用に遷移）
    /// </summary>
    void PostDraw();

    /// <summary>
    /// エフェクトをすべて処理し、最終結果をレンダーテクスチャ内に確定する
    /// </summary>
    void ProcessEffects();

    /// <summary>
    /// 2D / UI描画前処理（ポストエフェクト適用後の最終テクスチャをクリアせずにRenderTargetとしてバインド）
    /// </summary>
    void PreDraw2D();

    /// <summary>
    /// 2D / UI描画後処理（最終テクスチャを読み込み用リソース状態へ戻す）
    /// </summary>
    void PostDraw2D();

    /// <summary>
    /// 現在保持している最終結果テクスチャのSRVハンドルを返す（軽量ゲッター）
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetFinalSrvGpuHandle() const;

    /// <summary>
    /// 現在保持している最終結果テクスチャのリソースを返す
    /// </summary>
    ID3D12Resource* GetFinalResource() const;

    /// <summary>
    /// 現在保持している最終結果テクスチャへのポインタを返す
    /// </summary>
    RenderTexture* GetFinalRenderTexture() const;

    /// <summary>
    /// 指定されたレンダーターゲットに最終結果を描画コピーする
    /// </summary>
    void Draw(D3D12_CPU_DESCRIPTOR_HANDLE targetRtv);

    /// <summary>
    /// スワップチェーン（最終画面）への全画面コピー描画
    /// </summary>
    void Draw();

    /// <summary>
    /// 最終テクスチャをスワップチェーンへコピー描画する（エフェクト再処理なし）
    /// </summary>
    void CopyToSwapChain(D3D12_CPU_DESCRIPTOR_HANDLE targetRtv);
    void CopyToSwapChain();

    /// <summary>
    /// ImGui制御（全パスのImGuiControlを順次呼び出す）
    /// </summary>
    void ImGuiControl();

    // --- ウィンドウ表示制御 ---
    static bool* GetShowWindowPtr() { return &showWindow_; }
    static bool IsWindowOpen() { return showWindow_; }
    static void SetWindowOpen(bool open) { showWindow_ = open; }

    /// <summary>
    /// 背景クリアカラーモード設定
    /// </summary>
    void SetClearColorMode(PostClearColorMode mode);

    /// <summary>
    /// 現在の背景クリアカラーモードを取得
    /// </summary>
    PostClearColorMode GetClearColorMode() const { return clearColorMode_; }

    // 各個別パスのアクティブ制御アクセサ
    void SetGrayscaleActive(bool active);
    bool IsGrayscaleActive() const;

    void SetSepiaActive(bool active);
    bool IsSepiaActive() const;

    void SetVignetteActive(bool active);
    bool IsVignetteActive() const;

    void SetBoxFilterActive(bool active);
    bool IsBoxFilterActive() const;

    void SetGaussianBlurActive(bool active);
    bool IsGaussianBlurActive() const;

    void SetUnderwaterActive(bool active);
    bool IsUnderwaterActive() const;

    void SetDepthOutlineActive(bool active);
    bool IsDepthOutlineActive() const;

    void SetRadialBlurActive(bool active);
    bool IsRadialBlurActive() const;

    void SetDissolveActive(bool active);
    bool IsDissolveActive() const;

    void SetTVNoiseActive(bool active);
    bool IsTVNoiseActive() const;

    /// <summary>
    /// 全てのエフェクトを一括で無効化する
    /// </summary>
    void ClearEffects();
    void Resize(uint32_t width, uint32_t height);

    // ビネットパラメータのアクセサ（後方互換または直接コントロール用）
    void SetVignetteScale(float scale);
    float GetVignetteScale() const;

    void SetVignetteExponent(float exponent);
    float GetVignetteExponent() const;

    // BoxFilterパラメータのアクセサ
    void SetBoxFilterKernelRadius(int32_t radius);
    int32_t GetBoxFilterKernelRadius() const;

    // GaussianFilterパラメータのアクセサ
    void SetGaussianBlurParams(int32_t radius, float sigma);
    int32_t GetGaussianBlurKernelRadius() const;
    float GetGaussianBlurSigma() const;

    // DepthOutlineパラメータのアクセサ
    void SetDepthOutlineParams(float width, float threshold, float scale, const Vector3& color);
    float GetDepthOutlineEdgeWidth() const;
    float GetDepthOutlineThreshold() const;
    float GetDepthOutlineScale() const;
    Vector3 GetDepthOutlineEdgeColor() const;

    // RadialBlurパラメータのアクセサ
    void SetRadialBlurParams(const Vector2& center, float blurWidth);
    Vector2 GetRadialBlurCenter() const;
    float GetRadialBlurWidth() const;

    // Dissolveパラメータのアクセサ
    void SetDissolveParams(float threshold, float edgeWidth, const Vector3& edgeColor);
    float GetDissolveThreshold() const;
    float GetDissolveEdgeWidth() const;
    Vector3 GetDissolveEdgeColor() const;
    void SetDissolveActiveNoiseIndex(int32_t index);
    int32_t GetDissolveActiveNoiseIndex() const;

    // TVNoiseパラメータのアクセサ
    void SetTVNoiseStrength(float strength);
    float GetTVNoiseStrength() const;

private:
    static inline bool showWindow_ = true;

    std::unique_ptr<RenderTexture> renderTexture_;
    std::unique_ptr<RenderTexture> renderTextureTemp_; // ピンポン用の中間テクスチャ
    PostClearColorMode clearColorMode_ = PostClearColorMode::Blue;

    // ポストプロセスの各パスをリストで保持します
    std::vector<std::unique_ptr<IPostProcessPass>> passes_;
};
