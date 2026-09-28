#pragma once
#include <memory>
#include <vector>
#include "App/Scene/Game/Stage/Floor.h"
#include "App/Scene/Game/Stage/Pillar.h"

/**
 * @brief 床と柱を統括し、ステージ全体を表現・管理するクラス
 */
class Stage {
public:
    Stage();
    ~Stage() = default;

    void Initialize();
    void Update();
    
    // showWarning: 警告サークルを描画するかどうか (DrawRouteモードでtrue)
    // isPlayMode: プレイモード中かどうか (Drawでのカリング制御に使用)
    // playerPos: プレイヤーの位置 (プレイモード中のカリング用)
    void Draw(bool showWarning, bool isPlayMode, const Vector3& playerPos);

    // 柱のゲッター (衝突判定などで外部参照される場合用)
    const std::vector<std::unique_ptr<Pillar>>& GetPillars() const { return pillars_; }

private:
    std::unique_ptr<Floor> floor_;
    std::vector<std::unique_ptr<Pillar>> pillars_;

    // マジックナンバー排除のための定数
    static inline const float kPillarStartZ = -240.0f;
    static inline const float kPillarEndZ = 240.0f;
    static inline const float kPillarStepZ = 10.0f;
    static inline const float kPillarY = 5.0f;
    static inline const float kPillarXOffset = 12.0f;
    static constexpr int kPillarPeriodZ = 20;

    static inline const float kCullingDistance = 60.0f; // プレイ中の描画カリング距離
};
