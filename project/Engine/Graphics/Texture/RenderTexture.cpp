#include "Engine/Graphics/Texture/RenderTexture.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/Log/Log.h"
#include <cassert>
#include <format>

void RenderTexture::Initialize(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU,
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU,
    const std::wstring& name) {

    device_ = device;
    width_ = width;
    height_ = height;
    format_ = format;
    clearColor_ = clearColor;
    rtvHandleCPU_ = rtvHandle;
    srvHandleCPU_ = srvHandleCPU;
    srvHandleGPU_ = srvHandleGPU;
    name_ = name;

    // 1. テクスチャリソースの作成（初期状態は D3D12_RESOURCE_STATE_RENDER_TARGET）
    resource_ = DxUtils::CreateRenderTextureResource(device_, width_, height_, format_, clearColor_);
    assert(resource_ != nullptr && "Failed to create render texture resource in Initialize.");
    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // 2. RTV（RenderTargetView）の作成
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    
    device->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandleCPU_);

    // 3. SRV（ShaderResourceView）の作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);

    Log::Write(std::format(L" ├─ 【リソース生成完了 : {}】 レンダーテクスチャ (幅: {}, 高さ: {}, フォーマット: {}) の生成に成功しました。", name_, width_, height_, (int)format_));
}

void RenderTexture::Recreate(const Vector4& newClearColor) {
    assert(device_ != nullptr);
    clearColor_ = newClearColor;

    // 古いリソースを解放
    resource_.Reset();

    // 新しいクリアカラーでテクスチャリソースを再作成
    resource_ = DxUtils::CreateRenderTextureResource(device_, width_, height_, format_, clearColor_);
    assert(resource_ != nullptr && "Failed to recreate render texture resource.");
    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // RTVの再作成（同じディスクリプタハンドルに上書き）
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device_->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandleCPU_);

    // SRVの再作成（同じディスクリプタハンドルに上書き）
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);

    Log::Write(std::format(L" ├─ 【リソース再生成完了 : {}】 新しいクリアカラー ({:.2f}, {:.2f}, {:.2f}, {:.2f}) でレンダーテクスチャを再作成しました。", name_, clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w));
}


void RenderTexture::PreDraw(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, bool clearDepth) {
    // 描画ターゲットとするために RENDER_TARGET 状態にバリア遷移
    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = currentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // レンダーターゲットの設定
    commandList->OMSetRenderTargets(1, &rtvHandleCPU_, false, &dsvHandle);

    // クリア最適化値をそのまま使用してクリア
    float clearColorRGBA[4] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
    commandList->ClearRenderTargetView(rtvHandleCPU_, clearColorRGBA, 0, nullptr);

    if (clearDepth) {
        // 深度バッファのクリア（マジックナンバーの排除ルールを適用）
        const float kClearDepthValue = 1.0f;
        const uint8_t kClearStencilValue = 0;
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, kClearDepthValue, kClearStencilValue, 0, nullptr);
    }
}

void RenderTexture::PreDrawWithoutClear(ID3D12GraphicsCommandList* commandList) {
    // 描画ターゲットとするために RENDER_TARGET 状態にバリア遷移
    if (currentState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = currentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // クリアは行わずにレンダーターゲットのみを設定（2D UI描画のため深度バッファはバインドしない）
    commandList->OMSetRenderTargets(1, &rtvHandleCPU_, false, nullptr);
}

void RenderTexture::PostDraw(ID3D12GraphicsCommandList* commandList) {
    // シェーダで読み込むために PIXEL_SHADER_RESOURCE 状態にバリア遷移
    if (currentState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = currentState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &barrier);
        currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void RenderTexture::Resize(uint32_t width, uint32_t height) {
    if (width_ == width && height_ == height) return;
    width_ = width;
    height_ = height;
    Recreate(clearColor_);
}

