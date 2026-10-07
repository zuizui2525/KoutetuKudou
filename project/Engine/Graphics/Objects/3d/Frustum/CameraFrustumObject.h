#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

/// <summary>
/// カメラの視野角(FOV)・アスペクト比を視覚化するワイヤーフレーム視錐台(Frustum)ギズモモデル
/// </summary>
class CameraFrustumObject : public Object3D {
public:
    CameraFrustumObject() = default;
    ~CameraFrustumObject() override;

    /// <summary>
    /// 初期化（ライティング無効モード）
    /// </summary>
    void Initialize(int lightingMode = 0);

    /// <summary>
    /// 毎フレームの更新処理
    /// </summary>
    void Update() override;

    /// <summary>
    /// ワイヤーフレーム描画処理
    /// </summary>
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");

    /// <summary>
    /// カメラパラメータの設定（変更があった場合メッシュを再生成）
    /// </summary>
    void SetParameters(float fov, float aspectRatio, float nearZ = 0.2f, float farZ = 3.0f);

    float GetFov() const { return fov_; }
    float GetAspectRatio() const { return aspectRatio_; }
    float GetNearZ() const { return nearZ_; }
    float GetFarZ() const { return farZ_; }

private:
    void CreateMesh();
    void UpdateVertices();

private:
    // GPU リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    VertexData* mappedVertices_ = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // 視錐台パラメータ (デフォルト値)
    float fov_ = 0.45f;
    float aspectRatio_ = 16.0f / 9.0f;
    float nearZ_ = 0.2f;
    float farZ_ = 3.0f;

    bool needsUpdate_ = false;
};
