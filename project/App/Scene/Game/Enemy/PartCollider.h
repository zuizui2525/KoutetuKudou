#pragma once
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Math/Collision/CollisionStructs.h"

/**
 * @brief キャラクターの部位ごとの当たり判定を管理する空のオブジェクトクラス
 */
class PartCollider : public Object3D {
public:
    enum class Type {
        Body, // 胴体
        Head  // 頭部 (弱点)
    };

public:
    PartCollider() = default;
    ~PartCollider() override = default;

    /**
     * @brief 初期化処理
     * @param type 部位のタイプ
     * @param localSize 基準となるローカルのAABBサイズ
     */
    void Initialize(Type type, const Vector3& localSize);

    /**
     * @brief 行列更新処理
     */
    void Update() override;

    /**
     * @brief ワールド空間でのAABBを取得する
     */
    AABB GetWorldAABB() const;

    // ゲッター/セッター
    Type GetType() const { return type_; }
    const Vector3& GetLocalSize() const { return localSize_; }

private:
    // マジックナンバー排除用の係数
    static inline const float kHalf = 0.5f;

private:
    Type type_ = Type::Body;
    Vector3 localSize_ = { 1.0f, 1.0f, 1.0f };
};
