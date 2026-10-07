#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <memory>
#include <cstdint>
#include "Engine/Math/MathStructs.h"

/**
 * @brief テキスト文字列からDirectX12テクスチャを動的生成・キャッシュ管理するジェネレータ
 * Windows GDI による高品質フォントレンダリングを行い、白文字＋アルファチャンネルのテクスチャを生成します。
 */
class TextTextureGenerator {
public:
    TextTextureGenerator();
    ~TextTextureGenerator();

    /// @brief テキスト設定からテクスチャを生成（パラメータに変更がなければキャッシュを維持）
    /// @param text 表示文字列 (UTF-8)
    /// @param fontName フォントファミリ名 (例: "Yu Gothic UI", "Meiryo", "Arial")
    /// @param fontSize フォントサイズ (pt/px)
    /// @param alignment 揃え (0: 左揃え, 1: 中央揃え, 2: 右揃え)
    /// @param isBold 太字フラグ
    /// @param isItalic 斜体フラグ
    /// @param enableOutline アウトライン（縁取り）有効フラグ
    /// @param outlineColor アウトライン色 (RGBA)
    /// @param outlineWidth アウトラインの太さ (ピクセル単位)
    /// @param textColor テキスト本体色 (RGBA, アウトライン有効時に使用)
    /// @return テクスチャが更新または新規生成されたか (true: 更新あり, false: キャッシュ維持)
    bool Generate(
        const std::string& text,
        const std::string& fontName = "Yu Gothic UI",
        float fontSize = 32.0f,
        int alignment = 0,
        bool isBold = false,
        bool isItalic = false,
        bool enableOutline = false,
        const Vector4& outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f },
        float outlineWidth = 2.0f,
        const Vector4& textColor = { 1.0f, 1.0f, 1.0f, 1.0f }
    );

    /// @brief 生成されたテクスチャのGPUディスクリプタハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return gpuHandle_; }

    /// @brief テクスチャの幅（ピクセル）
    uint32_t GetWidth() const { return textureWidth_; }

    /// @brief テクスチャの高さ（ピクセル）
    uint32_t GetHeight() const { return textureHeight_; }

    /// @brief テクスチャリソースの取得
    ID3D12Resource* GetResource() const { return textureResource_.Get(); }

    /// @brief テクスチャが有効に生成されているか
    bool IsValid() const { return textureResource_ != nullptr && gpuHandle_.ptr != 0; }

private:
    // 定数定義 (マジックナンバー排除)
    static constexpr uint32_t kInvalidDescriptorIndex = 0xFFFFFFFF;
    static constexpr uint32_t kMinTextureDimension = 4;
    static constexpr uint32_t kPaddingPixels = 4;
    static constexpr uint32_t kBitsPerPixel = 32;
    static constexpr uint32_t kBytesPerPixel = 4;

    // 前回の生成パラメータ（キャッシュ比較用）
    std::string cachedText_ = "";
    std::string cachedFontName_ = "";
    float cachedFontSize_ = 0.0f;
    int cachedAlignment_ = -1;
    bool cachedIsBold_ = false;
    bool cachedIsItalic_ = false;
    bool cachedEnableOutline_ = false;
    Vector4 cachedOutlineColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
    float cachedOutlineWidth_ = 0.0f;
    Vector4 cachedTextColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};
    uint32_t descriptorIndex_ = kInvalidDescriptorIndex;

    uint32_t textureWidth_ = 0;
    uint32_t textureHeight_ = 0;
};
