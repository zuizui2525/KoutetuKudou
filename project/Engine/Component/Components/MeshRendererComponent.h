#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <memory>
#include <string>

class Object3D;

/// <summary>
/// 3Dメッシュ描画を担うコンポーネント (Unity/UnrealライクなMeshRenderer)
/// </summary>
class MeshRendererComponent : public IComponent {
public:
    enum class MeshType {
        Cube,
        Sphere,
        Pyramid,
        TriangularPyramid,
        Triangle,
        Square,
        Cylinder,
        Cone,
        Ring,
        Hemisphere,
        Model,
        Line,
        FlatRing,
        CylinderEffect
    };

    MeshRendererComponent();
    ~MeshRendererComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "MeshRenderer"; }

    // ==========================================
    // 形状・マテリアル設定
    // ==========================================
    MeshType GetMeshType() const { return meshType_; }
    void SetMeshType(MeshType type);

    const std::string& GetTextureKey() const { return textureKey_; }
    void SetTextureKey(const std::string& key) { textureKey_ = key; }

    const std::string& GetModelKey() const { return modelKey_; }
    void SetModelKey(const std::string& key) { modelKey_ = key; }

    const std::string& GetEnvMapKey() const { return envMapKey_; }
    void SetEnvMapKey(const std::string& key) { envMapKey_ = key; }

    const Vector4& GetColor() const { return color_; }
    void SetColor(const Vector4& color);

    int GetLightingMode() const { return lightingMode_; }
    void SetLightingMode(int mode);

    float GetShininess() const { return shininess_; }
    void SetShininess(float shininess);

private:
    void RecreateMeshObject();

private:
    MeshType meshType_ = MeshType::Cube;
    std::unique_ptr<Object3D> meshObject_;

    std::string textureKey_ = "white";
    std::string modelKey_ = "cube";
    std::string envMapKey_ = "";

    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    int lightingMode_ = 2; // 0:None, 1:Lambert, 2:HalfLambert (デフォルト: HalfLambert)
    float shininess_ = 30.0f;
};
