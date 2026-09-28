#include "App/Scene/Game/Enemy/PartCollider.h"
#include "Engine/Math/Matrix/Matrix.h"

void PartCollider::Initialize(Type type, const Vector3& localSize) {
    // 描画を行わないため、ライティングは無効
    Object3D::Initialize(0);

    type_ = type;
    localSize_ = localSize;

    // デバッグ表示用の名前を設定
    if (type_ == Type::Head) {
        SetName("HeadCollider");
    } else {
        SetName("BodyCollider");
    }
}

AABB PartCollider::GetWorldAABB() const {
    // ワールド位置の取得
    Vector3 worldPos = GetWorldPosition();

    // ワールドスケールの計算（親がいる場合は再帰的に乗算）
    Vector3 worldScale = transform_.scale;
    Object3D* currentParent = parent_;
    while (currentParent) {
        Vector3 parentScale = currentParent->GetScale();
        worldScale.x *= parentScale.x;
        worldScale.y *= parentScale.y;
        worldScale.z *= parentScale.z;
        currentParent = currentParent->GetParent();
    }

    // AABB のハーフサイズを算出
    Vector3 halfSize = {
        localSize_.x * worldScale.x * kHalf,
        localSize_.y * worldScale.y * kHalf,
        localSize_.z * worldScale.z * kHalf
    };

    // AABB 構造体を構築して返す
    AABB aabb;
    aabb.min = { worldPos.x - halfSize.x, worldPos.y - halfSize.y, worldPos.z - halfSize.z };
    aabb.max = { worldPos.x + halfSize.x, worldPos.y + halfSize.y, worldPos.z + halfSize.z };
    return aabb;
}

void PartCollider::Update() {
    // 親オブジェクトがある場合は親のワールド行列を乗算してワールド行列を更新
    Matrix4x4 world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    if (parent_) {
        world = Math::Multiply(world, parent_->GetWorldMatrix());
    }
    matWorld_ = world;
}
