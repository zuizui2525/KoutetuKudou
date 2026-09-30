#pragma once
#include <d3d12.h>
#include <string>
#include <cstdint>

class Object3D;

/**
 * @brief 3Dオブジェクトの描画処理を一括管理する共通描画クラス
 * 各種Object3D派生クラスから重複していたDirectX12パイプライン設定・CBV/SRV設定を集約します。
 */
class Object3DDrawer {
public:
    /// @brief シングルトンインスタンスの取得
    static Object3DDrawer* GetInstance();

    /// @brief 頂点インデックスを使用しない描画 (DrawInstanced)
    /// @param object 描画対象のObject3D
    /// @param vbView 頂点バッファビュー
    /// @param vertexCount 頂点数
    /// @param textureKey テクスチャキー名 (デフォルト: "white")
    /// @param envMapKey 環境マップキー名 (省略時は空文字)
    /// @param psoKey 使用するPSO/RootSignatureのキー名 (デフォルト: "Object3D")
    void Draw(
        Object3D* object,
        const D3D12_VERTEX_BUFFER_VIEW& vbView,
        uint32_t vertexCount,
        const std::string& textureKey = "white",
        const std::string& envMapKey = "",
        const std::string& psoKey = "Object3D"
    );

    /// @brief 頂点インデックスを使用する描画 (DrawIndexedInstanced)
    /// @param object 描画対象のObject3D
    /// @param vbView 頂点バッファビュー
    /// @param ibView インデックスバッファビュー
    /// @param indexCount インデックス数
    /// @param textureKey テクスチャキー名 (デフォルト: "white")
    /// @param envMapKey 環境マップキー名 (省略時は空文字)
    /// @param psoKey 使用するPSO/RootSignatureのキー名 (デフォルト: "Object3D")
    void DrawIndexed(
        Object3D* object,
        const D3D12_VERTEX_BUFFER_VIEW& vbView,
        const D3D12_INDEX_BUFFER_VIEW& ibView,
        uint32_t indexCount,
        const std::string& textureKey = "white",
        const std::string& envMapKey = "",
        const std::string& psoKey = "Object3D"
    );

private:
    Object3DDrawer() = default;
    ~Object3DDrawer() = default;
    Object3DDrawer(const Object3DDrawer&) = delete;
    Object3DDrawer& operator=(const Object3DDrawer&) = delete;

    /// @brief 描画共通のパイプラインおよび定数バッファ・テクスチャの設定
    bool PrepareDraw(
        Object3D* object,
        const D3D12_VERTEX_BUFFER_VIEW& vbView,
        const std::string& textureKey,
        const std::string& envMapKey,
        const std::string& psoKey,
        ID3D12GraphicsCommandList*& outCommandList
    );
};
