#pragma once
#include <d3d12.h>
#include <string>
#include <cstdint>

/**
 * @brief 2Dオブジェクトの描画処理を一括管理する共通描画クラス
 * SpriteObjectなど2DオブジェクトのDirectX12パイプライン設定・CBV/SRV設定を集約します。
 */
class Object2DDrawer {
public:
    static Object2DDrawer* GetInstance();

    /// @brief 頂点インデックスを使用する2D描画 (DrawIndexedInstanced)
    /// @param wvpResource WVP定数バッファ
    /// @param materialResource マテリアル定数バッファ
    /// @param vbView 頂点バッファビュー
    /// @param ibView インデックスバッファビュー
    /// @param indexCount インデックス数
    /// @param textureKey テクスチャキー名
    /// @param isVisible 表示フラグ
    /// @param psoKey 使用するPSO/RootSignatureのキー名 (デフォルト: "Object2D")
    void DrawIndexed(
        ID3D12Resource* wvpResource,
        ID3D12Resource* materialResource,
        const D3D12_VERTEX_BUFFER_VIEW& vbView,
        const D3D12_INDEX_BUFFER_VIEW& ibView,
        uint32_t indexCount,
        const std::string& textureKey,
        bool isVisible = true,
        const std::string& psoKey = "Object2D"
    );

    /// @brief GPUディスクリプタハンドル直接指定による2D描画
    void DrawIndexedHandle(
        ID3D12Resource* wvpResource,
        ID3D12Resource* materialResource,
        const D3D12_VERTEX_BUFFER_VIEW& vbView,
        const D3D12_INDEX_BUFFER_VIEW& ibView,
        uint32_t indexCount,
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
        bool isVisible = true,
        const std::string& psoKey = "Object2D"
    );

private:
    Object2DDrawer() = default;
    ~Object2DDrawer() = default;
    Object2DDrawer(const Object2DDrawer&) = delete;
    Object2DDrawer& operator=(const Object2DDrawer&) = delete;
};
