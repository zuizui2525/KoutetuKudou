#pragma once
#include <memory>
#include <string>
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Square/SquareObject.h"

/**
 * @brief マップの柱および警告線を管理するクラス
 */
class Pillar {
public:
    Pillar();
    ~Pillar() = default;

    void Initialize(const Vector3& position);
    void Update();
    void Draw(bool showWarning);

    const Vector3& GetPosition() const { return cube_->GetPosition(); }
    const Vector3& GetSize() const { return cube_->GetSize(); }

private:
    std::unique_ptr<CubeObject> cube_;
    std::unique_ptr<SquareObject> warningCircle_;

    // マジックナンバー排除のための定数
    static inline const Vector3 kPillarScale = { 2.0f, 10.0f, 2.0f };
    static inline const Vector4 kPillarColor = { 0.6f, 0.6f, 0.7f, 1.0f };

    static inline const Vector2 kWarningCircleSize = { 8.0f, 8.0f }; // 半径4.0f相当（直径8.0f）
    static inline const Vector3 kWarningCircleRotation = { 1.57f, 0.0f, 0.0f }; // 水平にする
    static inline const Vector4 kWarningColor = { 1.0f, 0.3f, 0.0f, 1.0f }; // オレンジの警告色
    static inline const std::string kWarningTextureKey = "circle_solid";
    static inline const float kWarningHeightOffset = 0.01f; // 床とのチラつき防止用のYオフセット
};
