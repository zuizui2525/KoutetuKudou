#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "Engine/Math/MathStructs.h"

/// <summary>
/// 文字列（UTF-8）からWindows GDIを用いて文字輪郭・ドット座標点群を抽出するサンプラークラス
/// </summary>
class TextShapeSampler {
public:
    // マジックナンバー排除用定数定義
    static constexpr uint32_t kDefaultFontSize = 64;
    static constexpr uint32_t kMinBitmapDimension = 64;
    static constexpr uint32_t kMaxBitmapDimension = 1024;
    static constexpr uint32_t kMaxSamplePoints = 4096;
    static constexpr uint8_t  kLuminanceThreshold = 120; // ドットとして認識する輝度閾値
    static constexpr int      kSampleStep = 2;           // 走査時のピクセルステップ

    TextShapeSampler() = default;
    ~TextShapeSampler() = default;

    /// <summary>
    /// 文字列からドット点群（-1.0〜1.0正規化座標）を抽出・生成します。
    /// 前回の文字列と同じであればキャッシュされた点群を即座に返します。
    /// </summary>
    /// <param name="text">抽出対象の文字列 (UTF-8)</param>
    /// <param name="fontName">使用フォント名 (既定: "Yu Gothic UI")</param>
    /// <param name="fontSize">ラスタライズフォントサイズ</param>
    /// <returns>抽出された正規化2D点群配列への参照</returns>
    const std::vector<Vector2>& ExtractPoints(
        const std::string& text,
        const std::string& fontName = "Yu Gothic UI",
        uint32_t fontSize = kDefaultFontSize
    );

    /// <summary>
    /// 現在キャッシュされている点群の取得
    /// </summary>
    const std::vector<Vector2>& GetCachedPoints() const { return cachedPoints_; }

    /// <summary>
    /// 点群数の取得
    /// </summary>
    size_t GetPointCount() const { return cachedPoints_.size(); }

private:
    std::string cachedText_ = "";
    std::string cachedFontName_ = "";
    uint32_t cachedFontSize_ = 0;
    std::vector<Vector2> cachedPoints_;
};

