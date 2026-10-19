#pragma once
#include <string>
#include <cstdint>
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 画面遷移（トランジション）の定数群（マジックナンバー排除）
/// </summary>
namespace TransitionConstants {
    constexpr float kDefaultSingleDuration = 0.5f; // 単体（INまたはOUT）のデフォルト秒数
    constexpr float kMinDuration = 0.05f;
    constexpr float kMaxDuration = 5.0f;

    constexpr float kDefaultMaxAlpha = 1.0f;
    constexpr float kMinAlpha = 0.0f;
    constexpr float kMaxAlpha = 1.0f;

    constexpr float kDefaultBlurStrength = 1.0f;
    constexpr float kMaxRadialBlurWidth = 0.08f;
    constexpr float kMaxGaussianSigma = 8.0f;
    constexpr int32_t kMaxGaussianRadius = 8;
    constexpr float kDefaultDissolveEdgeWidth = 0.05f;

    // デフォルトカラー（黒・完全不透明）
    constexpr Vector4 kDefaultBlackColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    // ディゾルブ発光エッジのデフォルトカラー（温かみのあるオレンジ発光）
    constexpr Vector3 kDefaultDissolveEdgeColor = { 1.0f, 0.6f, 0.1f };
}

/// <summary>
/// 画面遷移エフェクトの種類（PowerPointライク＋ゲームポストエフェクト）
/// </summary>
enum class TransitionEffectType : int32_t {
    None = 0,          // なし（即座に切り替え）
    ColorFade,         // カラーフェード（自由な色・透明度での暗転/明転）
    Dissolve,          // ディゾルブ（発光エッジとともに溶け落ちる）
    RadialBlur,        // 放射状ブラー（画面中央へのワープ・加速感）
    GaussianBlur,      // ガウシアンブラー（ピンボケ・フォーカスアウト）
    TVNoise,           // テレビノイズ（砂嵐・グリッチ・走査線）
    GrayscaleFade,     // モノクロフェード（脱色・回想演出）
    WipeLeft,          // 左ワイプ（画面が左へ払われる）
    WipeRight,         // 右ワイプ（画面が右へ払われる）
};

/// <summary>
/// 単一フェーズ（入場 または 退場）のトランジション設定構造体
/// </summary>
struct SingleTransitionSetting {
    bool enabled = false;                                                    // この演出が有効かどうか
    TransitionEffectType effectType = TransitionEffectType::ColorFade;       // 演出種類
    float duration = TransitionConstants::kDefaultSingleDuration;            // 所要秒数
    Vector4 color = TransitionConstants::kDefaultBlackColor;                 // RGB: 遷移色, A: 最大不透明度
    float blurStrength = TransitionConstants::kDefaultBlurStrength;         // エフェクト強度倍率
    Vector3 edgeColor = TransitionConstants::kDefaultDissolveEdgeColor;      // ディゾルブ発光色
    float edgeWidth = TransitionConstants::kDefaultDissolveEdgeWidth;        // ディゾルブ境界幅

    // 有効かつ演出がNoneでないか判定
    bool IsActive() const {
        return enabled && effectType != TransitionEffectType::None;
    }
};

/// <summary>
/// シーンごとの画面切り替え設定（IN: 入場演出 / OUT: 退場演出 を独立保持）
/// </summary>
struct SceneTransitionConfig {
    SingleTransitionSetting outSetting; // 退場演出（このシーンから抜けるとき）
    SingleTransitionSetting inSetting;  // 入場演出（このシーンに入るとき）

    // OUT設定が有効か判定
    bool HasOut() const { return outSetting.IsActive(); }

    // IN設定が有効か判定
    bool HasIn() const { return inSetting.IsActive(); }

    // ヘルパー: 表示用ラベル文字列の取得
    static const char* GetEffectTypeName(TransitionEffectType type) {
        switch (type) {
        case TransitionEffectType::None:          return "なし (None)";
        case TransitionEffectType::ColorFade:     return "カラーフェード (Color Fade)";
        case TransitionEffectType::Dissolve:      return "ディゾルブ (Dissolve)";
        case TransitionEffectType::RadialBlur:    return "ラジアルブラー (Radial Blur)";
        case TransitionEffectType::GaussianBlur:  return "ガウスぼかし (Gaussian Blur)";
        case TransitionEffectType::TVNoise:       return "TVノイズ (TV Noise)";
        case TransitionEffectType::GrayscaleFade: return "モノクロフェード (Grayscale)";
        case TransitionEffectType::WipeLeft:      return "左ワイプ (Wipe Left)";
        case TransitionEffectType::WipeRight:     return "右ワイプ (Wipe Right)";
        default: return "不明 (Unknown)";
        }
    }
};
