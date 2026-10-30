#include "Engine/Graphics/Text/TextShapeSampler.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace {
    constexpr uint32_t kBitsPerPixel = 32;
    constexpr uint32_t kBytesPerPixel = 4;
    constexpr uint32_t kPadding = 8;
    constexpr float kHalf = 0.5f;
    constexpr float kPointsToPixelsFactor = 1.333333f;
}

const std::vector<Vector2>& TextShapeSampler::ExtractPoints(
    const std::string& text,
    const std::string& fontName,
    uint32_t fontSize
) {
    if (text.empty()) {
        cachedText_ = "";
        cachedPoints_.clear();
        return cachedPoints_;
    }

    // キャッシュチェック（文字列・フォント・サイズが同一なら即座に返却）
    if (cachedText_ == text && cachedFontName_ == fontName && cachedFontSize_ == fontSize) {
        return cachedPoints_;
    }

    cachedText_ = text;
    cachedFontName_ = fontName;
    cachedFontSize_ = fontSize;
    cachedPoints_.clear();

    std::wstring textW = ConvertString(text);
    std::wstring fontNameW = ConvertString(fontName);

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) return cachedPoints_;

    int fontHeight = static_cast<int>(static_cast<float>(fontSize) * kPointsToPixelsFactor);
    HFONT hFont = CreateFontW(
        fontHeight, 0, 0, 0,
        FW_HEAVY, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontNameW.c_str()
    );

    HGDIOBJ hOldFont = SelectObject(hdc, hFont);

    // 文字寸法の計測
    RECT calcRect = { 0, 0, 0, 0 };
    DrawTextW(hdc, textW.c_str(), -1, &calcRect, DT_CALCRECT | DT_NOPREFIX);

    uint32_t textWidth = static_cast<uint32_t>((std::max)(1L, calcRect.right - calcRect.left));
    uint32_t textHeight = static_cast<uint32_t>((std::max)(1L, calcRect.bottom - calcRect.top));

    uint32_t bmpWidth = textWidth + kPadding * 2;
    uint32_t bmpHeight = textHeight + kPadding * 2;

    bmpWidth = (std::clamp)(bmpWidth, kMinBitmapDimension, kMaxBitmapDimension);
    bmpHeight = (std::clamp)(bmpHeight, kMinBitmapDimension, kMaxBitmapDimension);

    // DIB Section 作成
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(bmpWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(bmpHeight); // トップダウン
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = kBitsPerPixel;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBitmap || !pBits) {
        if (hFont) DeleteObject(hFont);
        DeleteDC(hdc);
        return cachedPoints_;
    }

    HGDIOBJ hOldBmp = SelectObject(hdc, hBitmap);
    memset(pBits, 0, bmpWidth * bmpHeight * kBytesPerPixel);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    RECT drawRect = {
        static_cast<LONG>(kPadding),
        static_cast<LONG>(kPadding),
        static_cast<LONG>(bmpWidth - kPadding),
        static_cast<LONG>(bmpHeight - kPadding)
    };

    DrawTextW(hdc, textW.c_str(), -1, &drawRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // ピクセル走査とドット点群の抽出
    auto* pixels = reinterpret_cast<const uint8_t*>(pBits);
    std::vector<Vector2> rawPoints;

    float minX = static_cast<float>(bmpWidth);
    float maxX = 0.0f;
    float minY = static_cast<float>(bmpHeight);
    float maxY = 0.0f;

    for (uint32_t y = 0; y < bmpHeight; y += kSampleStep) {
        for (uint32_t x = 0; x < bmpWidth; x += kSampleStep) {
            uint32_t idx = (y * bmpWidth + x) * kBytesPerPixel;
            uint8_t b = pixels[idx + 0];
            uint8_t g = pixels[idx + 1];
            uint8_t r = pixels[idx + 2];
            uint8_t luminance = static_cast<uint8_t>((static_cast<uint32_t>(r) * 77 + static_cast<uint32_t>(g) * 150 + static_cast<uint32_t>(b) * 29) >> 8);

            if (luminance >= kLuminanceThreshold) {
                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);
                rawPoints.push_back({ fx, fy });

                if (fx < minX) minX = fx;
                if (fx > maxX) maxX = fx;
                if (fy < minY) minY = fy;
                if (fy > maxY) maxY = fy;
            }
        }
    }

    // クリーンアップ
    SelectObject(hdc, hOldBmp);
    SelectObject(hdc, hOldFont);
    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdc);

    if (rawPoints.empty()) {
        return cachedPoints_;
    }

    // 正規化 (-1.0f 〜 1.0f) へのスケーリング
    float centerX = (minX + maxX) * kHalf;
    float centerY = (minY + maxY) * kHalf;
    // 縦幅を基準（-1.0f 〜 1.0f）としつつ、文字数が多い時は夜空の画面枠に収まるよう自動縮小フィット
    constexpr float kMaxAspectExtent = 2.2f; // 夜空の横幅比率の上限

    float halfHeight = (maxY - minY) * kHalf;
    float halfWidth  = (maxX - minX) * kHalf;
    if (halfHeight <= 0.0001f) halfHeight = 1.0f;

    float boundExtent = halfHeight;
    if (halfWidth / halfHeight > kMaxAspectExtent) {
        boundExtent = halfWidth / kMaxAspectExtent;
    }



    if (boundExtent <= 0.0001f) boundExtent = 1.0f;

    cachedPoints_.reserve(rawPoints.size());
    for (const auto& pt : rawPoints) {
        // 3D空間では上が +Y なので、画像のY軸（下向き）を反転
        float nx = (pt.x - centerX) / boundExtent;
        float ny = (centerY - pt.y) / boundExtent;
        cachedPoints_.push_back({ nx, ny });
    }

    return cachedPoints_;
}

