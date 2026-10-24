#include "Engine/Graphics/Objects/3d/Model/ModelObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Zuizui.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Graphics/Objects/3d/Model/ModelManager.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <cassert>
#include <cmath>

namespace {
    constexpr float kDefaultBoneThickness = 0.02f;
    constexpr Vector4 kBoneLineColor = { 0.2f, 0.9f, 1.0f, 1.0f }; // 視認性の高いシアン色
    constexpr const char* kBoneLineTextureKey = "white";
    constexpr float kDeltaTime = 1.0f / 60.0f;
    constexpr float kZero = 0.0f;
    constexpr int kLineLightingMode = 0;
}

void ModelObject::Initialize(int lightingMode) {
    Object3D::Initialize(lightingMode);
    animationLocalMatrix_ = Math::MakeIdentity();
}

void ModelObject::PlayAnimation(const std::string& animationFilePath, bool loop) {
    if (loadedAnimationPath_ != animationFilePath) {
        currentAnimation_ = AnimationHelper::LoadAnimationFile(animationFilePath);
        loadedAnimationPath_ = animationFilePath;
    }
    isAnimationPlaying_ = true;
    isAnimationLoop_ = loop;
    animationTime_ = kZero;

    // スケルトンが未初期化の場合、現在のモデルから生成
    if (!currentModelKey_.empty()) {
        UpdateSkeleton(currentModelKey_);
    }
}

void ModelObject::StopAnimation() {
    isAnimationPlaying_ = false;
    animationTime_ = kZero;
    animationLocalMatrix_ = Math::MakeIdentity();

    // スケルトンを初期姿勢に戻す
    if (hasSkeleton_) {
        ::Update(skeleton_);
    }
}

void ModelObject::UpdateSkeleton(const std::string& modelKey) {
    auto modelData = sModelMgr->GetModelData(modelKey);
    if (!modelData || modelData->rootNode.name.empty()) return;

    skeleton_ = CreateSkeleton(modelData->rootNode);
    hasSkeleton_ = true;
    EnsureSkeletonLines();

    // スキンクラスタの構築 (Step 4: GPUスキニング)
    if (!modelData->skinClusterData.empty()) {
        skinCluster_ = CreateSkinCluster(sEngine->GetDevice(), skeleton_, *modelData);
        hasSkinCluster_ = skinCluster_.isValid;
    }
}

void ModelObject::EnsureSkeletonLines() {
    if (!hasSkeleton_) return;

    size_t jointCount = skeleton_.joints.size();
    if (boneLines_.size() < jointCount) {
        boneLines_.resize(jointCount);
        for (size_t i = 0; i < jointCount; ++i) {
            if (!boneLines_[i]) {
                boneLines_[i] = std::make_unique<LineObject>();
                boneLines_[i]->Initialize(kLineLightingMode);
                boneLines_[i]->SetThickness(kDefaultBoneThickness);
                boneLines_[i]->GetMaterialData()->color = kBoneLineColor;
            }
        }
    }
}

void ModelObject::Update() {
    // スケルトンがまだ作られていなければ生成を試みる
    if (!hasSkeleton_ && !currentModelKey_.empty()) {
        UpdateSkeleton(currentModelKey_);
    }

    // アニメーション再生中の場合、時刻を進めてスケルトンおよびルートノードに反映
    if (isAnimationPlaying_ && currentAnimation_.duration > kZero) {
        animationTime_ += kDeltaTime;

        if (isAnimationLoop_) {
            animationTime_ = std::fmod(animationTime_, currentAnimation_.duration);
        } else if (animationTime_ > currentAnimation_.duration) {
            animationTime_ = currentAnimation_.duration;
        }

        // 1. スケルトンへのアニメーション適用と階層行列更新 (資料02.Skeleton スライド14, 17)
        if (hasSkeleton_) {
            ApplyAnimation(skeleton_, currentAnimation_, animationTime_);
            ::Update(skeleton_);

            // スキンクラスタ（マトリックスパレット）の更新 (Step 4: GPUスキニング)
            if (hasSkinCluster_) {
                UpdateSkinCluster(skinCluster_, skeleton_);
            }
        }

        // 2. ルートノード名に対応するアニメーションを抽出して localMatrix に合成 (Step 1基盤)
        if (!currentModelKey_.empty()) {
            auto modelData = sModelMgr->GetModelData(currentModelKey_);
            if (modelData && !modelData->rootNode.name.empty()) {
                auto it = currentAnimation_.nodeAnimations.find(modelData->rootNode.name);
                if (it != currentAnimation_.nodeAnimations.end()) {
                    const NodeAnimation& rootAnim = it->second;
                    Vector3 translate = AnimationHelper::CalculateValue(rootAnim.translate, animationTime_);
                    Quaternion rotate = AnimationHelper::CalculateValue(rootAnim.rotate, animationTime_);
                    Vector3 scale = AnimationHelper::CalculateValue(rootAnim.scale, animationTime_);
                    animationLocalMatrix_ = Math::MakeAffineMatrix(scale, rotate, translate);
                }
            }
        }
    }

    // 基底クラスObject3DのUpdateを実行（親行列parentMatrix_やTransformの乗算を確実に行う）
    Object3D::Update();

    // アニメーション行列を matWorld_ に合成 (スキニングモデルでない場合のみルート姿勢を適用)
    if (isAnimationPlaying_ && !hasSkinCluster_) {
        matWorld_ = Math::Multiply(animationLocalMatrix_, matWorld_);
    }
}

void ModelObject::Draw(const std::string& modelKey, const std::string& textureKey, const std::string& envMapKey) {
    if (!isVisible_ || modelKey.empty()) return;

    if (currentModelKey_ != modelKey) {
        currentModelKey_ = modelKey;
        UpdateSkeleton(modelKey);
    }

    auto modelData = sModelMgr->GetModelData(modelKey);
    if (!modelData) return;

    std::string finalTextureKey = textureKey;
    if (finalTextureKey.empty()) {
        finalTextureKey = modelKey;
    }

    // 描画処理は共通描画クラス Object3DDrawer を通して実行 (Step 4: GPUスキニング対応)
    if (hasSkinCluster_ && skinCluster_.isValid && !modelData->indices.empty() && modelData->indexResource) {
        uint32_t indexCount = static_cast<uint32_t>(modelData->indices.size());
        Object3DDrawer::GetInstance()->DrawSkinning(
            this,
            modelData->vbv,
            skinCluster_.influenceBufferView,
            modelData->ibv,
            indexCount,
            skinCluster_.paletteSrvHandleGPU,
            finalTextureKey,
            envMapKey
        );
    } else if (!modelData->indices.empty() && modelData->indexResource) {
        uint32_t indexCount = static_cast<uint32_t>(modelData->indices.size());
        Object3DDrawer::GetInstance()->DrawIndexed(this, modelData->vbv, modelData->ibv, indexCount, finalTextureKey, envMapKey);
    } else {
        uint32_t vertexCount = static_cast<uint32_t>(modelData->vertices.size());
        Object3DDrawer::GetInstance()->Draw(this, modelData->vbv, vertexCount, finalTextureKey, envMapKey);
    }

    // ボーン可視化デバッグ描画 (資料02.Skeleton スライド15, 18)
    if (showSkeleton_ && hasSkeleton_) {
        DrawSkeletonDebug();
    }
}

void ModelObject::DrawSkeletonDebug() {
    EnsureSkeletonLines();

    for (size_t i = 0; i < skeleton_.joints.size(); ++i) {
        const auto& joint = skeleton_.joints[i];
        if (!joint.parent) continue; // 親がいないルートジョイントは親との接続線を描画しない

        int32_t parentIndex = *joint.parent;
        if (parentIndex < 0 || parentIndex >= static_cast<int32_t>(skeleton_.joints.size())) continue;

        const auto& parentJoint = skeleton_.joints[parentIndex];

        // 各Jointのワールド座標 (skeletonSpaceMatrix * matWorld_)
        Matrix4x4 childWorld = Math::Multiply(joint.skeletonSpaceMatrix, matWorld_);
        Matrix4x4 parentWorld = Math::Multiply(parentJoint.skeletonSpaceMatrix, matWorld_);

        Vector3 childPos = { childWorld.m[3][0], childWorld.m[3][1], childWorld.m[3][2] };
        Vector3 parentPos = { parentWorld.m[3][0], parentWorld.m[3][1], parentWorld.m[3][2] };

        if (i < boneLines_.size() && boneLines_[i]) {
            boneLines_[i]->SetStartPoint(parentPos);
            boneLines_[i]->SetEndPoint(childPos);
            boneLines_[i]->Update();
            boneLines_[i]->Draw(kBoneLineTextureKey);
        }
    }
}

