#pragma once
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Engine/Math/MathStructs.h"

class RenderTexture {
public:
    RenderTexture() = default;
    ~RenderTexture() = default;

    // 初期化（レンダーテクスチャリソースの作成、RTV・SRVの作成）
    void Initialize(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU,
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU,
        const std::wstring& name);

    // 描画前処理（状態をRENDER_TARGETに遷移し、レンダーターゲットの設定およびクリア）
    void PreDraw(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, bool clearDepth = true);

    // 描画前処理（クリアを行わずにレンダーターゲットへ遷移・設定し、2Dスプライト等のオーバーレイ描画を可能にする）
    void PreDrawWithoutClear(ID3D12GraphicsCommandList* commandList);

    // 描画後処理（状態をPIXEL_SHADER_RESOURCEに遷移してシェーダ読み込み可能にする）
    void PostDraw(ID3D12GraphicsCommandList* commandList);

    // 指定された新しいクリアカラーでリソースを再作成する
    void Recreate(const Vector4& newClearColor);
    void Resize(uint32_t width, uint32_t height);

    // ゲッター
    ID3D12Resource* GetResource() const { return resource_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return srvHandleGPU_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCpuHandle() const { return rtvHandleCPU_; }

private:
    ID3D12Device* device_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleCPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    Vector4 clearColor_{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::wstring name_;

    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
};
