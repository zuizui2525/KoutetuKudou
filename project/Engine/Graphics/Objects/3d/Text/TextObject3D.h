#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/MathStructs.h"

/**
 * @brief 3Dテキスト表示用平面オブジェクト (ビルボード対応)
 * ワールド空間に配置可能で、カメラに対する正対（ビルボード）を制御できます。
 */
class TextObject3D : public Object3D {
public:
    enum class BillboardMode {
        None,       // ビルボードなし (通常の3Dトランスフォームに従う)
        AllAxis,    // 全軸カメラ正対 (常にカメラを向く: 頭上ネームタグ、浮遊UI等)
        YAxisOnly   // 垂直軸のみカメラ追従 (Y軸回転のみカメラを向く: 立て看板等)
    };

    TextObject3D();
    ~TextObject3D() override;

    void Initialize(int lightingMode = 0);
    void Update() override;
    void Draw(
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
        const std::string& envMapKey = "",
        const std::string& psoKey = "Object3D"
    );

    // サイズ・ビルボード
    const Vector2& GetSize() const { return size_; }
    void SetSize(const Vector2& size);

    BillboardMode GetBillboardMode() const { return billboardMode_; }
    void SetBillboardMode(BillboardMode mode) { billboardMode_ = mode; }

private:
    // メッシュ定数 (マジックナンバー排除)
    static constexpr uint32_t kVertexCount = 4;
    static constexpr uint32_t kIndexCount = 6;
    static constexpr float kDefaultWidth = 2.0f;
    static constexpr float kDefaultHeight = 0.5f;

    void CreateMesh();

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};

    Vector2 size_ = { kDefaultWidth, kDefaultHeight };
    BillboardMode billboardMode_ = BillboardMode::AllAxis;
    bool needsMeshUpdate_ = false;
};
