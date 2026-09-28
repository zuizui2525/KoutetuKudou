#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

class SquareObject : public Object3D {
public:
    SquareObject() = default;
    ~SquareObject() = default;

    void Initialize(int lightingMode = 2);

    // 更新処理
    void Update();

    // 描画処理
    void Draw(const std::string& textureKey = "white", const std::string& envMapKey = "", const std::string& psoKey = "Object3D");

    // Getter
    Vector2 GetSize() const { return size_; }

    // Setter
    void SetSize(const Vector2& size) { size_ = size; needsUpdate_ = true; }

private:
    void CreateMesh();

private:
    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    // バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    // パラメータ
    Vector2 size_ = { 1.0f, 1.0f };
    bool needsUpdate_ = false; // 再生成フラグ
};
