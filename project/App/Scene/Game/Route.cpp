#include "App/Scene/Game/Route.h"
#include "Engine/Input/Input.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Zuizui.h"
#include <cmath>
#include <algorithm>

Route::Route() {
    startSphere_ = std::make_unique<SphereObject>();
    goalSphere_ = std::make_unique<SphereObject>();
}

void Route::Initialize(Input* input, CameraManager* cameraMgr) {
    input_ = input;
    cameraMgr_ = cameraMgr;

    // スタート地点とゴール地点の視覚用球体オブジェクトの初期化
    startSphere_->Initialize();
    startSphere_->SetScale({ kAreaRadius * 2.0f, kAreaRadius * 2.0f, kAreaRadius * 2.0f });
    startSphere_->SetColor({ 1.0f, 1.0f, 0.0f, 0.5f }); // 黄色（半透明）

    goalSphere_->Initialize();
    goalSphere_->SetScale({ kAreaRadius * 2.0f, kAreaRadius * 2.0f, kAreaRadius * 2.0f });
    goalSphere_->SetColor({ 0.0f, 0.5f, 1.0f, 0.5f }); // 青色（半透明）

    CreateBatchResources();
    Reset();
}

void Route::CreateBatchResources() {
    auto device = EngineResource::GetEngine()->GetDevice();

    // 頂点バッファ
    size_t maxVertices = kMaxSegments * kVerticesPerSegment;
    batchVertexResource_ = DxUtils::CreateBufferResource(device, sizeof(VertexData) * maxVertices);
    batchVbView_.BufferLocation = batchVertexResource_->GetGPUVirtualAddress();
    batchVbView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * maxVertices);
    batchVbView_.StrideInBytes = sizeof(VertexData);

    // インデックスバッファ
    size_t maxIndices = kMaxSegments * kIndicesPerSegment;
    batchIndexResource_ = DxUtils::CreateBufferResource(device, sizeof(uint32_t) * maxIndices);
    batchIbView_.BufferLocation = batchIndexResource_->GetGPUVirtualAddress();
    batchIbView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * maxIndices);
    batchIbView_.Format = DXGI_FORMAT_R32_UINT;

    // インデックス初期データの割り当て (セグメントごとに 0,1,2, 2,1,3)
    uint32_t* idxGPU = nullptr;
    batchIndexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));
    for (uint32_t i = 0; i < static_cast<uint32_t>(kMaxSegments); ++i) {
        idxGPU[i * 6 + 0] = i * 4 + 0;
        idxGPU[i * 6 + 1] = i * 4 + 1;
        idxGPU[i * 6 + 2] = i * 4 + 2;
        idxGPU[i * 6 + 3] = i * 4 + 2;
        idxGPU[i * 6 + 4] = i * 4 + 1;
        idxGPU[i * 6 + 5] = i * 4 + 3;
    }
    batchIndexResource_->Unmap(0, nullptr);

    // WVP定数バッファ
    batchWvpResource_ = DxUtils::CreateBufferResource(device, sizeof(TransformationMatrix));
    batchWvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&batchWvpData_));
    batchWvpData_->WVP = Math::MakeIdentity();
    batchWvpData_->world = Math::MakeIdentity();
    batchWvpData_->WorldInverseTranspose = Math::MakeIdentity();

    // マテリアル定数バッファ
    batchMaterialResource_ = DxUtils::CreateBufferResource(device, sizeof(Material));
    batchMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&batchMaterialData_));
    batchMaterialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    batchMaterialData_->enableLighting = 0;
    batchMaterialData_->uvtransform = Math::MakeIdentity();
    batchMaterialData_->shininess = 1.0f;
    batchMaterialData_->environmentCoefficient = 0.0f;
}

void Route::Reset() {
    ClearForNewArea();
    SetupArea(0);
}

void Route::Update(BaseCamera* activeCamera) {
    // マウスドラッグによるルートの記録
    if (input_->MousePress(0)) { // 左クリック
        Vector2 mousePos = GameViewWindow::GetMousePosition();
        Vector2 viewSize = GameViewWindow::GetGameViewSize();

        // 画面全体の30%の左側領域のみ操作を受け付ける
        static const float kLeftAreaRate = 0.3f;
        if (mousePos.x <= viewSize.x * kLeftAreaRate) {
            Vector3 rayStart, rayDir;
            activeCamera->CreateRay(mousePos, viewSize.x * kLeftAreaRate, viewSize.y, rayStart, rayDir);

            if (std::abs(rayDir.y) > 0.0001f) {
                float t = (kPlaneIntersectY - rayStart.y) / rayDir.y;
                if (t >= 0.0f) {
                    Vector3 intersectPos = Math::Add(rayStart, Math::Multiply(t, rayDir));

                    // マップ範囲内に入っているかチェック
                    if (std::abs(intersectPos.x) <= kMapBoundaryX && intersectPos.z >= currentAreaStartZ_ && intersectPos.z <= currentAreaGoalZ_) {
                        if (rawPoints_.empty()) {
                            // 最初の一点はスタートエリア付近のみ許可
                            float toStartX = intersectPos.x - 0.0f;
                            float toStartZ = intersectPos.z - currentAreaStartZ_;
                            float distToStartSq = toStartX * toStartX + toStartZ * toStartZ;

                            if (distToStartSq <= kAreaRadius * kAreaRadius) {
                                ClearForNewArea();
                                isDrawing_ = true;
                                rawPoints_.push_back(intersectPos);
                            }
                        } else {
                            if (!hasReachedGoal_) {
                                Vector3 diff = Math::Subtract(intersectPos, rawPoints_.back());
                                float dist = Math::Length(diff);
                                if (dist >= kMinPointDistance) {
                                    // ゴールエリアに到達したかチェック
                                    float toGoalX = intersectPos.x - 0.0f;
                                    float toGoalZ = intersectPos.z - currentAreaGoalZ_;
                                    float distToGoalSq = toGoalX * toGoalX + toGoalZ * toGoalZ;

                                    rawPoints_.push_back(intersectPos);

                                    lineSegments_.push_back({ rawPoints_[rawPoints_.size() - 2], rawPoints_.back(), kLineThickness, kLineColor });

                                    // ゴールエリアに入ったら終了
                                    if (distToGoalSq <= kAreaRadius * kAreaRadius) {
                                        hasReachedGoal_ = true;
                                        isDrawing_ = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        isDrawing_ = false;
    }
}

void Route::Update2D(const Vector3& intersectPos) {
    if (input_->MousePress(0)) {
        if (std::abs(intersectPos.x) <= kMapBoundaryX && intersectPos.z >= currentAreaStartZ_ && intersectPos.z <= currentAreaGoalZ_) {
            // ドラッグ開始時にスタート位置の近くをクリックした場合は線をクリアして書き直す
            if (input_->MouseTrigger(0)) {
                float toStartX = intersectPos.x - 0.0f;
                float toStartZ = intersectPos.z - currentAreaStartZ_;
                float distToStartSq = toStartX * toStartX + toStartZ * toStartZ;
                if (distToStartSq <= kAreaRadius * kAreaRadius) {
                    ClearForNewArea();
                }
            }

            if (rawPoints_.empty()) {
                ClearForNewArea();
                isDrawing_ = true;
                
                // 1点目はスタート地点固定
                Vector3 startPos = { 0.0f, 0.0f, currentAreaStartZ_ };
                rawPoints_.push_back(startPos);
                
                // 2点目として現在のドラッグ位置を追加
                rawPoints_.push_back(intersectPos);

                lineSegments_.push_back({ startPos, intersectPos, kLineThickness, kLineColor });
            } else {
                if (!hasReachedGoal_) {
                    Vector3 diff = Math::Subtract(intersectPos, rawPoints_.back());
                    float dist = Math::Length(diff);
                    if (dist >= kMinPointDistance) {
                        float toGoalX = intersectPos.x - 0.0f;
                        float toGoalZ = intersectPos.z - currentAreaGoalZ_;
                        float distToGoalSq = toGoalX * toGoalX + toGoalZ * toGoalZ;

                        rawPoints_.push_back(intersectPos);

                        lineSegments_.push_back({ rawPoints_[rawPoints_.size() - 2], rawPoints_.back(), kLineThickness, kLineColor });

                        if (distToGoalSq <= kAreaRadius * kAreaRadius) {
                            hasReachedGoal_ = true;
                            isDrawing_ = false;
                        }
                    }
                }
            }
        }
    } else {
        isDrawing_ = false;
    }

    startSphere_->Update();
    goalSphere_->Update();
}

void Route::UpdateSpheres() {
    startSphere_->Update();
    goalSphere_->Update();
}

void Route::Draw() {
    size_t totalSegments = editorGizmoSegments_.size() + lineSegments_.size();
    if (totalSegments == 0) return;
    if (totalSegments > kMaxSegments) {
        totalSegments = kMaxSegments;
    }

    // カメラ位置の取得
    Matrix4x4 viewMat = CameraResource::GetCameraManager()->GetViewMatrix3D();
    Matrix4x4 viewInv = Math::Inverse(viewMat);
    Vector3 cameraPos = { viewInv.m[3][0], viewInv.m[3][1], viewInv.m[3][2] };

    // WVP行列の更新
    Matrix4x4 world = Math::MakeIdentity();
    Matrix4x4 wvp = Math::Multiply(Math::Multiply(world, viewMat), CameraResource::GetCameraManager()->GetProjectionMatrix3D());
    batchWvpData_->WVP = wvp;
    batchWvpData_->world = world;
    batchWvpData_->WorldInverseTranspose = world;

    // 頂点バッファヘ書き込み
    VertexData* vtx = nullptr;
    batchVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

    struct BatchDrawRange {
        Vector4 color;
        uint32_t indexCount;
        uint32_t startIndex;
    };
    std::vector<BatchDrawRange> batches;

    auto processSegment = [&](const LineSegmentData& seg, size_t segIdx) {
        Vector3 lineVec = Math::Subtract(seg.endPoint, seg.startPoint);
        Vector3 centerPos = {
            seg.startPoint.x + lineVec.x * 0.5f,
            seg.startPoint.y + lineVec.y * 0.5f,
            seg.startPoint.z + lineVec.z * 0.5f
        };
        Vector3 viewVec = Math::Subtract(centerPos, cameraPos);

        Vector3 normal = {
            lineVec.y * viewVec.z - lineVec.z * viewVec.y,
            lineVec.z * viewVec.x - lineVec.x * viewVec.z,
            lineVec.x * viewVec.y - lineVec.y * viewVec.x
        };
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0001f) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }
        float halfThickness = seg.thickness * 0.5f;
        Vector3 offset = { normal.x * halfThickness, normal.y * halfThickness, normal.z * halfThickness };

        size_t baseVtx = segIdx * 4;
        vtx[baseVtx + 0].position = { seg.startPoint.x - offset.x, seg.startPoint.y - offset.y, seg.startPoint.z - offset.z, 1.0f };
        vtx[baseVtx + 0].normal = { 0.0f, 1.0f, 0.0f };
        vtx[baseVtx + 0].texcoord = { 0.0f, 1.0f };

        vtx[baseVtx + 1].position = { seg.endPoint.x - offset.x, seg.endPoint.y - offset.y, seg.endPoint.z - offset.z, 1.0f };
        vtx[baseVtx + 1].normal = { 0.0f, 1.0f, 0.0f };
        vtx[baseVtx + 1].texcoord = { 0.0f, 0.0f };

        vtx[baseVtx + 2].position = { seg.startPoint.x + offset.x, seg.startPoint.y + offset.y, seg.startPoint.z + offset.z, 1.0f };
        vtx[baseVtx + 2].normal = { 0.0f, 1.0f, 0.0f };
        vtx[baseVtx + 2].texcoord = { 1.0f, 1.0f };

        vtx[baseVtx + 3].position = { seg.endPoint.x + offset.x, seg.endPoint.y + offset.y, seg.endPoint.z + offset.z, 1.0f };
        vtx[baseVtx + 3].normal = { 0.0f, 1.0f, 0.0f };
        vtx[baseVtx + 3].texcoord = { 1.0f, 0.0f };

        uint32_t startIndex = static_cast<uint32_t>(segIdx * kIndicesPerSegment);
        if (batches.empty() || batches.back().color.x != seg.color.x || batches.back().color.y != seg.color.y || batches.back().color.z != seg.color.z || batches.back().color.w != seg.color.w) {
            batches.push_back({ seg.color, kIndicesPerSegment, startIndex });
        } else {
            batches.back().indexCount += kIndicesPerSegment;
        }
    };

    size_t currentIdx = 0;
    for (const auto& seg : editorGizmoSegments_) {
        if (currentIdx >= totalSegments) break;
        processSegment(seg, currentIdx++);
    }
    for (const auto& seg : lineSegments_) {
        if (currentIdx >= totalSegments) break;
        processSegment(seg, currentIdx++);
    }

    batchVertexResource_->Unmap(0, nullptr);

    // CommandList による一括描画発行
    auto commandList = EngineResource::GetEngine()->GetDxCommon()->GetCommandList();
    commandList->SetGraphicsRootSignature(EngineResource::GetEngine()->GetPSOManager()->GetRootSignature("Object3D"));
    commandList->SetPipelineState(EngineResource::GetEngine()->GetPSOManager()->GetPSO("Object3D"));
    commandList->IASetVertexBuffers(0, 1, &batchVbView_);
    commandList->IASetIndexBuffer(&batchIbView_);
    commandList->SetGraphicsRootConstantBufferView(0, batchWvpResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, batchMaterialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, CameraResource::GetCameraManager()->GetGPUVirtualAddress());
    auto lightMgr = LightResource::GetLightManager();
    if (lightMgr) {
        commandList->SetGraphicsRootConstantBufferView(3, lightMgr->GetDirectionalLightGroupAddress());
        commandList->SetGraphicsRootConstantBufferView(4, lightMgr->GetPointLightGroupAddress());
        commandList->SetGraphicsRootConstantBufferView(5, lightMgr->GetSpotLightGroupAddress());
    }
    auto texMgr = TextureResource::GetTextureManager();
    if (texMgr) {
        commandList->SetGraphicsRootDescriptorTable(6, texMgr->GetGpuHandle("white"));
    }

    // 色ごとにマテリアル定数バッファを更新してDrawIndexedInstanced呼び出し
    for (const auto& batch : batches) {
        batchMaterialData_->color = batch.color;
        commandList->DrawIndexedInstanced(batch.indexCount, 1, batch.startIndex, 0, 0);
    }
}

void Route::DrawSpheres() {
    startSphere_->Draw();
    goalSphere_->Draw();
}

void Route::FinalizeRoute() {
    const int kPathDivision = 40; // 視覚的なカクつきをなくすため分割数を40に増加

    // rawPoints_のコピーを作成し、移動平均フィルタを複数回（5回）適用して急激なジグザグ・手ブレ角を丸める
    std::vector<Vector3> smoothedPoints = rawPoints_;
    if (smoothedPoints.size() >= 3) {
        for (int iter = 0; iter < 5; ++iter) {
            std::vector<Vector3> temp = smoothedPoints;
            for (size_t i = 1; i < smoothedPoints.size() - 1; ++i) {
                temp[i].x = (smoothedPoints[i - 1].x + smoothedPoints[i].x + smoothedPoints[i + 1].x) / 3.0f;
                temp[i].y = (smoothedPoints[i - 1].y + smoothedPoints[i].y + smoothedPoints[i + 1].y) / 3.0f;
                temp[i].z = (smoothedPoints[i - 1].z + smoothedPoints[i].z + smoothedPoints[i + 1].z) / 3.0f;
            }
            smoothedPoints = temp;
        }
    }

    pathPoints_ = Math::GenerateCatmullRomPath(smoothedPoints, kPathDivision);

    BuildEqualSpacingTable();
}

void Route::BuildEqualSpacingTable() {
    accumDistances_.clear();
    accumDistances_.push_back(0.0f);
    float accum = 0.0f;
    for (size_t i = 1; i < pathPoints_.size(); ++i) {
        float dist = Math::Length(Math::Subtract(pathPoints_[i], pathPoints_[i - 1]));
        accum += dist;
        accumDistances_.push_back(accum);
    }
    totalDistance_ = accum;
}

Vector3 Route::GetPositionAtDistance(float distance) const {
    if (pathPoints_.empty()) return { 0.0f, 0.0f, 0.0f };
    if (distance <= 0.0f) return pathPoints_.front();
    if (distance >= totalDistance_) return pathPoints_.back();

    size_t idx = 0;
    for (size_t i = 0; i < accumDistances_.size() - 1; ++i) {
        if (accumDistances_[i] <= distance && distance < accumDistances_[i + 1]) {
            idx = i;
            break;
        }
    }

    float tLocal = 0.0f;
    float distDiff = accumDistances_[idx + 1] - accumDistances_[idx];
    if (distDiff > 0.0001f) {
        tLocal = (distance - accumDistances_[idx]) / distDiff;
    }

    return Math::Add(pathPoints_[idx], Math::Multiply(tLocal, Math::Subtract(pathPoints_[idx + 1], pathPoints_[idx])));
}

Vector3 Route::GetTangentAtDistance(float distance) const {
    if (pathPoints_.size() < 2) return { 0.0f, 0.0f, 1.0f };
    
    float targetDist = std::clamp(distance, 0.0f, totalDistance_);
    size_t idx = 0;
    for (size_t i = 0; i < accumDistances_.size() - 1; ++i) {
        if (accumDistances_[i] <= targetDist && targetDist < accumDistances_[i + 1]) {
            idx = i;
            break;
        }
    }
    if (idx >= pathPoints_.size() - 1) idx = pathPoints_.size() - 2;

    return Math::Normalize(Math::Subtract(pathPoints_[idx + 1], pathPoints_[idx]));
}

Vector3 Route::GetRotationAtDistance(float distance) const {
    Vector3 tangent = GetTangentAtDistance(distance);
    float yaw = std::atan2(tangent.x, tangent.z);
    float pitch = -std::atan2(tangent.y, std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z));
    return { pitch, yaw, 0.0f };
}

void Route::AddGizmoRect(const Vector3& center, float width, float depth, const Vector4& color) {
    float hx = width * kHalf;
    float hz = depth * kHalf;

    Vector3 corners[4] = {
        { center.x - hx, 0.01f, center.z - hz },
        { center.x + hx, 0.01f, center.z - hz },
        { center.x + hx, 0.01f, center.z + hz },
        { center.x - hx, 0.01f, center.z + hz }
    };

    for (int i = 0; i < 4; ++i) {
        editorGizmoSegments_.push_back({ corners[i], corners[(i + 1) % 4], kGizmoThickness, color });
    }
}

void Route::AddGizmoCircle(const Vector3& center, float radius, const Vector4& color) {
    std::vector<Vector3> points;
    points.reserve(kCircleDivision);
    for (int i = 0; i < kCircleDivision; ++i) {
        float theta = (2.0f * kPi * i) / kCircleDivision;
        float x = center.x + radius * std::cos(theta);
        float z = center.z + radius * std::sin(theta);
        points.push_back({ x, 0.01f, z });
    }

    for (int i = 0; i < kCircleDivision; ++i) {
        editorGizmoSegments_.push_back({ points[i], points[(i + 1) % kCircleDivision], kGizmoThickness, color });
    }
}

void Route::SetupArea(int areaIndex) {
    currentAreaIndex_ = areaIndex;
    
    // エリアごとの定数
    static const float kAreaLength = 120.0f;
    currentAreaStartZ_ = kStartAreaZ + static_cast<float>(areaIndex) * kAreaLength;
    currentAreaGoalZ_ = currentAreaStartZ_ + kAreaLength;

    // スタート地点とゴール地点の球体座標更新
    startSphere_->SetPosition({ 0.0f, 1.0f, currentAreaStartZ_ });
    goalSphere_->SetPosition({ 0.0f, 1.0f, currentAreaGoalZ_ });

    SetupAreaGizmos();
}

void Route::SetupAreaGizmos() {
    editorGizmoSegments_.clear();

    // エリア境界枠（白）: Xはマップ左右外枠、Zは現在のエリア範囲
    float centerZ = (currentAreaStartZ_ + currentAreaGoalZ_) * kHalf;
    float lengthZ = currentAreaGoalZ_ - currentAreaStartZ_;
    AddGizmoRect({ 0.0f, 0.0f, centerZ }, kMapBoundaryX * 2.0f, lengthZ, { 1.0f, 1.0f, 1.0f, 1.0f });

    // スタート枠 (黄・円形)
    AddGizmoCircle({ 0.0f, 0.0f, currentAreaStartZ_ }, kAreaRadius, { 1.0f, 1.0f, 0.0f, 1.0f });
    // ゴール枠 (青・円形)
    AddGizmoCircle({ 0.0f, 0.0f, currentAreaGoalZ_ }, kAreaRadius, { 0.0f, 0.5f, 1.0f, 1.0f });

    // ボス出現ライン (エリア3のZ=360fに配置)
    static const float kBossSpawnLineZ = 180.0f;
    if (currentAreaIndex_ == 3) {
        // 赤い太めの横線を引く
        static const float kBossLineThickness = 0.5f;
        editorGizmoSegments_.push_back({ { -kMapBoundaryX, 0.01f, kBossSpawnLineZ }, { kMapBoundaryX, 0.01f, kBossSpawnLineZ }, kBossLineThickness, { 1.0f, 0.0f, 0.0f, 1.0f } });
    }
}

void Route::ClearForNewArea() {
    rawPoints_.clear();
    pathPoints_.clear();
    accumDistances_.clear();
    lineSegments_.clear();
    totalDistance_ = 0.0f;
    isDrawing_ = false;
    hasReachedGoal_ = false;
}

void Route::UpdateLines() {
    // 一括描画方式のため毎フレームの個別のLineObject更新は不要
}

void Route::SyncFrom(const Route* other) {
    currentAreaIndex_ = other->currentAreaIndex_;
    currentAreaStartZ_ = other->currentAreaStartZ_;
    currentAreaGoalZ_ = other->currentAreaGoalZ_;
    isDrawing_ = other->isDrawing_;
    hasReachedGoal_ = other->hasReachedGoal_;
    rawPoints_ = other->rawPoints_;
    pathPoints_ = other->pathPoints_;
    accumDistances_ = other->accumDistances_;
    totalDistance_ = other->totalDistance_;

    lineSegments_ = other->lineSegments_;
    editorGizmoSegments_ = other->editorGizmoSegments_;

    // 球体の同期
    startSphere_->SetPosition(other->startSphere_->GetPosition());
    goalSphere_->SetPosition(other->goalSphere_->GetPosition());
}

