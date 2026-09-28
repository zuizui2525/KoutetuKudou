#pragma once
#include <memory>
#include "Engine/Graphics/Objects/3d/Square/SquareObject.h"

/**
 * @brief マップの床オブジェクトを管理するクラス
 */
class Floor {
public:
    Floor();
    ~Floor() = default;

    void Initialize();
    void Update();
    void Draw();

private:
    std::unique_ptr<SquareObject> square_;

    // マジックナンバー排除のための定数
    static inline const Vector3 kDefaultPosition = { 0.0f, 0.0f, 0.0f };
    static inline const Vector2 kDefaultSize = { 25.0f, 1000.0f };
    static inline const Vector3 kDefaultRotation = { 1.57f, 0.0f, 0.0f }; // X軸で90度回転して水平にする
    static inline const Vector4 kDefaultColor = { 0.5f, 1.0f, 0.5f, 1.0f }; // 黄緑色
};
