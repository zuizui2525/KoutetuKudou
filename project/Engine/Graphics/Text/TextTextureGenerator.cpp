#include "Engine/Graphics/Text/TextTextureGenerator.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/Utils/StringUtility.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <vector>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace {
    // GDI描画・テクスチャ定数 (マジックナンバー排除)
    constexpr UINT kAlignmentLeft = 0;
    constexpr UINT kAlignmentCenter = 1;
    constexpr UINT kAlignmentRight = 2;

    constexpr uint32_t kDefaultTextureWidth = 4;
    constexpr uint32_t kDefaultTextureHeight = 4;
    constexpr float kPointsToPixelsFactor = 1.333333f;
    constexpr UINT kRowPitchAlignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT; // 256バイトアライメント
    constexpr UINT kMipLevels = 1;
    constexpr UINT kArraySize = 1;
    constexpr UINT kSampleCount = 1;

    // 白文字RGB
    constexpr uint8_t kWhiteChannel = 255;

    // アウトライン描画用定数
    constexpr float kOutlineAntiAliasFeather = 0.5f;
    constexpr float kByteToFloatScale = 1.0f / 255.0f;
    constexpr float kFloatToByteScale = 255.0f;
    constexpr float kAlphaEpsilonThreshold = 0.0001f;
}

TextTextureGenerator::TextTextureGenerator() = default;

TextTextureGenerator::~TextTextureGenerator() {
    if (textureResource_) {
        DeferredReleaseManager::GetInstance()->Enqueue(std::move(textureResource_));
    }
}

bool TextTextureGenerator::Generate(
    const std::string& text,
    const std::string& fontName,
    float fontSize,
    int alignment,
    bool isBold,
    bool isItalic,
    bool enableOutline,
    const Vector4& outlineColor,
    float outlineWidth,
    const Vector4& textColor
) {
    // キャッシュ判定：パラメータが前回と完全に一致していれば再生成をスキップ
    if (textureResource_ &&
        cachedText_ == text &&
        cachedFontName_ == fontName &&
        cachedFontSize_ == fontSize &&
        cachedAlignment_ == alignment &&
        cachedIsBold_ == isBold &&
        cachedIsItalic_ == isItalic &&
        cachedEnableOutline_ == enableOutline &&
        (!enableOutline || (
            cachedOutlineColor_.x == outlineColor.x &&
            cachedOutlineColor_.y == outlineColor.y &&
            cachedOutlineColor_.z == outlineColor.z &&
            cachedOutlineColor_.w == outlineColor.w &&
            cachedOutlineWidth_ == outlineWidth &&
            cachedTextColor_.x == textColor.x &&
            cachedTextColor_.y == textColor.y &&
            cachedTextColor_.z == textColor.z &&
            cachedTextColor_.w == textColor.w
        ))) {
        return false;
    }

    auto engine = EngineResource::GetEngine();
    if (!engine) return false;

    auto device = engine->GetDevice();
    auto dxCommon = engine->GetDxCommon();
    if (!device || !dxCommon) return false;

    auto commandQueue = dxCommon->GetCommandQueue();
    auto srvHeap = dxCommon->GetSrvHeap();
    auto texMgr = TextureResource::GetTextureManager();
    if (!commandQueue || !srvHeap || !texMgr) return false;

    // 1. テキスト寸法計測およびビットマップ作成 (Windows GDI)
    std::wstring textW = ConvertString(text);
    std::wstring fontNameW = ConvertString(fontName);

    HDC hdc = CreateCompatibleDC(nullptr);
    assert(hdc != nullptr);

    // フォントサイズの高さ計算（pt -> logical pixels）
    int fontHeight = static_cast<int>(fontSize * kPointsToPixelsFactor);
    if (fontHeight <= 0) fontHeight = static_cast<int>(fontSize);

    HFONT hFont = CreateFontW(
        -fontHeight,
        0,
        0,
        0,
        isBold ? FW_BOLD : FW_NORMAL,
        isItalic ? TRUE : FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontNameW.c_str()
    );

    HGDIOBJ hOldFont = SelectObject(hdc, hFont);

    // 文字列寸法の計測
    RECT calcRect = { 0, 0, 0, 0 };
    if (!textW.empty()) {
        DrawTextW(hdc, textW.c_str(), -1, &calcRect, DT_CALCRECT | DT_NOPREFIX);
    }

    uint32_t contentWidth = static_cast<uint32_t>((std::max)(0L, calcRect.right - calcRect.left));
    uint32_t contentHeight = static_cast<uint32_t>((std::max)(0L, calcRect.bottom - calcRect.top));

    // 余白パディングの付加 (アウトライン太さに応じて自動拡張)
    uint32_t outlineMargin = 0;
    if (enableOutline && outlineWidth > 0.0f) {
        outlineMargin = static_cast<uint32_t>(std::ceil(outlineWidth));
    }
    uint32_t totalPadding = kPaddingPixels + outlineMargin;
    uint32_t bmpWidth = contentWidth + totalPadding * 2;
    uint32_t bmpHeight = contentHeight + totalPadding * 2;

    if (bmpWidth < kMinTextureDimension) bmpWidth = kMinTextureDimension;
    if (bmpHeight < kMinTextureDimension) bmpHeight = kMinTextureDimension;

    // 32bpp DIB Section（トップダウン）を作成
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(bmpWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(bmpHeight); // トップダウン
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = kBitsPerPixel;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    assert(hBitmap != nullptr && pBits != nullptr);

    HGDIOBJ hOldBmp = SelectObject(hdc, hBitmap);

    // 背景を黒（RGB=0）で初期化
    memset(pBits, 0, bmpWidth * bmpHeight * kBytesPerPixel);

    if (!textW.empty()) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255)); // 白文字でラスタライズ

        UINT alignFlags = DT_LEFT;
        if (alignment == kAlignmentCenter) {
            alignFlags = DT_CENTER;
        } else if (alignment == kAlignmentRight) {
            alignFlags = DT_RIGHT;
        }

        RECT drawRect = {
            static_cast<LONG>(totalPadding),
            static_cast<LONG>(totalPadding),
            static_cast<LONG>(bmpWidth - totalPadding),
            static_cast<LONG>(bmpHeight - totalPadding)
        };

        DrawTextW(hdc, textW.c_str(), -1, &drawRect, alignFlags | DT_NOPREFIX);
    }

    GdiFlush();

    // 2. ピクセルデータを RGBA に変換
    std::vector<uint8_t> rgbaPixels(bmpWidth * bmpHeight * kBytesPerPixel, 0);
    const uint8_t* pSrcBits = static_cast<const uint8_t*>(pBits);

    if (!enableOutline) {
        // 通常モード: 白文字 + アンチエイリアス輝度をAlphaに変換
        for (uint32_t y = 0; y < bmpHeight; ++y) {
            for (uint32_t x = 0; x < bmpWidth; ++x) {
                size_t idx = (static_cast<size_t>(y) * bmpWidth + x) * kBytesPerPixel;
                uint8_t b = pSrcBits[idx + 0];
                uint8_t g = pSrcBits[idx + 1];
                uint8_t r = pSrcBits[idx + 2];

                uint8_t alpha = (std::max)({ r, g, b });

                rgbaPixels[idx + 0] = kWhiteChannel;
                rgbaPixels[idx + 1] = kWhiteChannel;
                rgbaPixels[idx + 2] = kWhiteChannel;
                rgbaPixels[idx + 3] = alpha;
            }
        }
    } else {
        // アウトライン合成モード: モルフォロジー膨張によるマスク抽出 & アルファブレンド
        // A. 本文不透明度マップ (0-255)
        std::vector<uint8_t> textAlpha(bmpWidth * bmpHeight, 0);
        for (uint32_t y = 0; y < bmpHeight; ++y) {
            for (uint32_t x = 0; x < bmpWidth; ++x) {
                size_t idx = (static_cast<size_t>(y) * bmpWidth + x) * kBytesPerPixel;
                uint8_t b = pSrcBits[idx + 0];
                uint8_t g = pSrcBits[idx + 1];
                uint8_t r = pSrcBits[idx + 2];
                textAlpha[y * bmpWidth + x] = (std::max)({ r, g, b });
            }
        }

        // B. モルフォロジー膨張によるアウトライン不透明度マップ生成
        std::vector<uint8_t> outlineAlpha(bmpWidth * bmpHeight, 0);
        int radiusInt = static_cast<int>(std::ceil(outlineWidth));
        float maxSearchDistSq = (outlineWidth + kOutlineAntiAliasFeather) * (outlineWidth + kOutlineAntiAliasFeather);

        for (int y = 0; y < static_cast<int>(bmpHeight); ++y) {
            for (int x = 0; x < static_cast<int>(bmpWidth); ++x) {
                float maxSampledAlpha = 0.0f;
                for (int dy = -radiusInt; dy <= radiusInt; ++dy) {
                    int ny = y + dy;
                    if (ny < 0 || ny >= static_cast<int>(bmpHeight)) continue;

                    for (int dx = -radiusInt; dx <= radiusInt; ++dx) {
                        int nx = x + dx;
                        if (nx < 0 || nx >= static_cast<int>(bmpWidth)) continue;

                        float distSq = static_cast<float>(dx * dx + dy * dy);
                        if (distSq <= maxSearchDistSq) {
                            float dist = std::sqrt(distSq);
                            float factor = 1.0f;
                            if (dist > outlineWidth - kOutlineAntiAliasFeather) {
                                factor = (outlineWidth + kOutlineAntiAliasFeather - dist) / (kOutlineAntiAliasFeather * 2.0f);
                                factor = (std::clamp)(factor, 0.0f, 1.0f);
                            }
                            float sampleVal = static_cast<float>(textAlpha[ny * bmpWidth + nx]) * factor;
                            if (sampleVal > maxSampledAlpha) {
                                maxSampledAlpha = sampleVal;
                            }
                        }
                    }
                }
                outlineAlpha[y * bmpWidth + x] = static_cast<uint8_t>((std::clamp)(maxSampledAlpha, 0.0f, 255.0f));
            }
        }

        // C. 本文色とアウトライン色の Porter-Duff Over アルファ合成
        for (uint32_t y = 0; y < bmpHeight; ++y) {
            for (uint32_t x = 0; x < bmpWidth; ++x) {
                size_t pixelIdx = static_cast<size_t>(y) * bmpWidth + x;
                size_t outIdx = pixelIdx * kBytesPerPixel;

                float textA = (static_cast<float>(textAlpha[pixelIdx]) * kByteToFloatScale) * textColor.w;
                float outA = (static_cast<float>(outlineAlpha[pixelIdx]) * kByteToFloatScale) * outlineColor.w;

                // 本文で覆われていないアウトラインの可視アルファ
                float visibleOutA = outA * (1.0f - textA);
                float finalA = textA + visibleOutA;

                if (finalA > kAlphaEpsilonThreshold) {
                    float finalR = (textColor.x * textA + outlineColor.x * visibleOutA) / finalA;
                    float finalG = (textColor.y * textA + outlineColor.y * visibleOutA) / finalA;
                    float finalB = (textColor.z * textA + outlineColor.z * visibleOutA) / finalA;

                    rgbaPixels[outIdx + 0] = static_cast<uint8_t>((std::clamp)(finalR * kFloatToByteScale, 0.0f, 255.0f));
                    rgbaPixels[outIdx + 1] = static_cast<uint8_t>((std::clamp)(finalG * kFloatToByteScale, 0.0f, 255.0f));
                    rgbaPixels[outIdx + 2] = static_cast<uint8_t>((std::clamp)(finalB * kFloatToByteScale, 0.0f, 255.0f));
                    rgbaPixels[outIdx + 3] = static_cast<uint8_t>((std::clamp)(finalA * kFloatToByteScale, 0.0f, 255.0f));
                } else {
                    rgbaPixels[outIdx + 0] = 0;
                    rgbaPixels[outIdx + 1] = 0;
                    rgbaPixels[outIdx + 2] = 0;
                    rgbaPixels[outIdx + 3] = 0;
                }
            }
        }
    }

    // GDIオブジェクトの解放
    SelectObject(hdc, hOldBmp);
    SelectObject(hdc, hOldFont);
    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdc);

    // 3. DirectX 12 テクスチャリソースの生成
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = bmpWidth;
    texDesc.Height = bmpHeight;
    texDesc.DepthOrArraySize = kArraySize;
    texDesc.MipLevels = kMipLevels;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = kSampleCount;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> newTexture;
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&newTexture)
    );
    assert(SUCCEEDED(hr));

    // 4. アップロードバッファの作成とデータコピー (RowPitch アライメント対応)
    UINT rowPitch = (bmpWidth * kBytesPerPixel + kRowPitchAlignment - 1) & ~(kRowPitchAlignment - 1);
    UINT uploadBufferSize = rowPitch * bmpHeight;

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer = DxUtils::CreateBufferResource(device, uploadBufferSize);

    uint8_t* pMappedUpload = nullptr;
    hr = uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pMappedUpload));
    assert(SUCCEEDED(hr));

    for (uint32_t y = 0; y < bmpHeight; ++y) {
        memcpy(
            pMappedUpload + (static_cast<size_t>(y) * rowPitch),
            rgbaPixels.data() + (static_cast<size_t>(y) * bmpWidth * kBytesPerPixel),
            static_cast<size_t>(bmpWidth) * kBytesPerPixel
        );
    }
    uploadBuffer->Unmap(0, nullptr);

    // 5. 即時コマンドリストによるGPU転送 & フェンス完了待機
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList));
    assert(SUCCEEDED(hr));

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = newTexture.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = uploadBuffer.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset = 0;
    srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srcLoc.PlacedFootprint.Footprint.Width = bmpWidth;
    srcLoc.PlacedFootprint.Footprint.Height = bmpHeight;
    srcLoc.PlacedFootprint.Footprint.Depth = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = rowPitch;

    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // コピー完了後に GENERIC_READ 状態へ遷移
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = newTexture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    cmdList->ResourceBarrier(1, &barrier);

    hr = cmdList->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* commandLists[] = { cmdList.Get() };
    commandQueue->ExecuteCommandLists(1, commandLists);

    // GPU同期待機
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    assert(SUCCEEDED(hr));
    commandQueue->Signal(fence.Get(), 1);

    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent != nullptr);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    CloseHandle(fenceEvent);

    // 6. 古いテクスチャリソースを安全に遅延解放キューへ送る
    if (textureResource_) {
        DeferredReleaseManager::GetInstance()->Enqueue(std::move(textureResource_));
    }
    textureResource_ = std::move(newTexture);

    // 7. ディスクリプタヒープのスロット確保（初回のみ）と SRV 作成
    if (descriptorIndex_ == kInvalidDescriptorIndex) {
        descriptorIndex_ = texMgr->AllocateDescriptorIndex();
        UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        cpuHandle_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, descriptorIndex_);
        gpuHandle_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, descriptorIndex_);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = kMipLevels;
    device->CreateShaderResourceView(textureResource_.Get(), &srvDesc, cpuHandle_);

    // 状態更新
    textureWidth_ = bmpWidth;
    textureHeight_ = bmpHeight;
    cachedText_ = text;
    cachedFontName_ = fontName;
    cachedFontSize_ = fontSize;
    cachedAlignment_ = alignment;
    cachedIsBold_ = isBold;
    cachedIsItalic_ = isItalic;
    cachedEnableOutline_ = enableOutline;
    cachedOutlineColor_ = outlineColor;
    cachedOutlineWidth_ = outlineWidth;
    cachedTextColor_ = textColor;

    return true;
}
