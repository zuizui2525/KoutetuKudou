#pragma once
#include "Engine/Graphics/Objects/3d/Object3D.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Animation/Animation.h"
#include "Engine/Graphics/Animation/Skeleton.h"
#include "Engine/Graphics/Animation/SkinCluster.h"
#include <string>
#include <vector>
#include <memory>

class LineObject;

class ModelObject : public Object3D, public BaseModel {
public:
    void Initialize(int lightingMode = 2);
    void Update();
    void Draw(const std::string& modelKey = "cube", const std::string& textureKey = "", const std::string& envMapKey = "");

    /// @brief アニメーションファイルの再生を開始
    void PlayAnimation(const std::string& animationFilePath, bool loop = true);
    /// @brief アニメーションを停止
    void StopAnimation();

    bool IsAnimationPlaying() const { return isAnimationPlaying_; }
    float GetAnimationTime() const { return animationTime_; }
    void SetAnimationTime(float time) { animationTime_ = time; }

    /// @brief スケルトン取得・設定
    Skeleton& GetSkeleton() { return skeleton_; }
    const Skeleton& GetSkeleton() const { return skeleton_; }
    bool HasSkeleton() const { return hasSkeleton_; }

    /// @brief ボーン可視化デバッグ描画のON/OFF
    bool IsShowSkeleton() const { return showSkeleton_; }
    void SetShowSkeleton(bool show) { showSkeleton_ = show; }

    /// @brief 特定のボーンの現在のワールド変換行列を取得（武器や防具の追従・アタッチ用）
    std::optional<Matrix4x4> GetJointWorldMatrix(const std::string& jointName) const;

    /// @brief 特定のボーンの現在のワールド座標を取得
    std::optional<Vector3> GetJointWorldPosition(const std::string& jointName) const;

private:
    void UpdateSkeleton(const std::string& modelKey);
    void DrawSkeletonDebug();
    void EnsureSkeletonLines();
    void ClearSkeletonAndSkin();

private:
    bool isAnimationPlaying_ = false;
    bool isAnimationLoop_ = true;
    float animationTime_ = 0.0f;
    Animation currentAnimation_;
    std::string loadedAnimationPath_;
    std::string currentModelKey_;
    Matrix4x4 animationLocalMatrix_{};

    // スケルトン（Step 2）
    Skeleton skeleton_{};
    bool hasSkeleton_ = false;
    bool showSkeleton_ = true;
    std::vector<std::unique_ptr<LineObject>> boneLines_;

    // スキンクラスタ（Step 4）
    SkinCluster skinCluster_{};
    bool hasSkinCluster_ = false;
};
