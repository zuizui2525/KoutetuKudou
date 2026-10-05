#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"
#include "Engine/Graphics/Objects/Light/Point/PointLight.h"
#include "Engine/Graphics/Objects/Light/Spot/SpotLight.h"
#include <string>

/// <summary>
/// Unityライクなライトコンポーネント
/// 平行光源(Directional)、点光源(Point)、スポットライト(Spot)を切り替え・管理する
/// 所属するGameObjectのTransform（位置・回転）と連動する
/// </summary>
class LightComponent : public IComponent {
public:
    enum class LightType {
        Directional,
        Point,
        Spot
    };

    LightComponent();
    ~LightComponent() override;

    void Initialize() override;
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "Light"; }

    // ==========================================
    // タイプ設定
    // ==========================================
    LightType GetLightType() const { return lightType_; }
    void SetLightType(LightType type) { lightType_ = type; }

    // ==========================================
    // 共通パラメータ
    // ==========================================
    const Vector4& GetColor() const { return color_; }
    void SetColor(const Vector4& color) { color_ = color; }

    float GetIntensity() const { return intensity_; }
    void SetIntensity(float intensity) { intensity_ = intensity; }

    // ==========================================
    // Point / Spot パラメータ
    // ==========================================
    float GetRange() const { return range_; }
    void SetRange(float range) { range_ = range; }

    float GetDecay() const { return decay_; }
    void SetDecay(float decay) { decay_ = decay; }

    // ==========================================
    // Spot 専用パラメータ
    // ==========================================
    float GetSpotAngle() const { return spotAngle_; }
    void SetSpotAngle(float angle) { spotAngle_ = angle; }

    float GetSpotFalloffStart() const { return spotFalloffStart_; }
    void SetSpotFalloffStart(float falloff) { spotFalloffStart_ = falloff; }

    // ==========================================
    // GPU転送用構造体データの取得
    // ==========================================
    DirectionalLight GetDirectionalLightData() const;
    PointLight GetPointLightData() const;
    SpotLight GetSpotLightData() const;

    /// <summary>
    /// 計算された照射方向ベクトルを取得
    /// </summary>
    Vector3 GetDirection() const;

private:
    void RegisterToLightManager();
    void UnregisterFromLightManager();

private:
    LightType lightType_ = LightType::Directional;

    // 共通設定
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    float intensity_ = 1.0f;

    // Point / Spot 共通 (届く最大距離・減衰)
    float range_ = 10.0f;   // Point: radius, Spot: distance
    float decay_ = 1.0f;

    // Spot 専用 (度数法)
    float spotAngle_ = 45.0f;
    float spotFalloffStart_ = 30.0f;

    bool isRegistered_ = false;
};
