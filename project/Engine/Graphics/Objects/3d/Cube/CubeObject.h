#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

class CubeObject : public Object3D {
public:
    CubeObject() = default;
    ~CubeObject() override;

    void Initialize(int lightingMode = 2);

    // 更新処理
    void Update() override;

    // 描画処理
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "");

    // Getter
    Vector3 GetSize() const { return size_; }

    // Setter
    void SetSize(const Vector3& size) { size_ = size; needsUpdate_ = true; }

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kVertexCount = 24;
    static constexpr uint32_t kIndexCount = 36;

    void CreateMesh();

private:
    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    // バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // パラメータ
    Vector3 size_ = { 1.0f, 1.0f, 1.0f };
    bool needsUpdate_ = false; // 再生成フラグ
};
