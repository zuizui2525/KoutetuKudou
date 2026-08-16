#include "App/Scene/Game/UI/Minimap.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Input/Input.h"
#include "Engine/Debug/GameViewWindow.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Zuizui.h"
#include "App/Scene/Game/Stage/Stage.h"
#include "App/Scene/Game/Route.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <cmath>
#include <algorithm>

Minimap::Minimap() = default;
Minimap::~Minimap() = default;

/**
 * @brief 初期化処理
 * @param stage ステージ障害物のポインタ（ミニマップに障害物アイコンを配置するため）
 */
void Minimap::Initialize(Stage* stage) {
    // 背景
    minimapBg_ = std::make_unique<SpriteObject>();
    minimapBg_->Initialize(0);
    minimapBg_->GetMaterialData()->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白背景

    // スタート
    startIcon_ = std::make_unique<SpriteObject>();
    startIcon_->Initialize(0);
    startIcon_->GetMaterialData()->color = { 0.1f, 0.8f, 0.1f, 1.0f }; // 緑

    // ゴール
    goalIcon_ = std::make_unique<SpriteObject>();
    goalIcon_->Initialize(0);
    goalIcon_->GetMaterialData()->color = { 0.0f, 0.5f, 1.0f, 1.0f }; // 青

    // 自機
    indicatorIcon_ = std::make_unique<SpriteObject>();
    indicatorIcon_->Initialize(0);
    indicatorIcon_->GetMaterialData()->color = { 1.0f, 0.0f, 0.0f, 1.0f }; // 赤

    // ボス戦エリア用の赤半透明
    bossArea2D_ = std::make_unique<SpriteObject>();
    bossArea2D_->Initialize(0);
    bossArea2D_->GetMaterialData()->color = kBossAreaColor;

    // 柱
    pillarIcons_.clear();
    if (stage) {
        const auto& pillars = stage->GetPillars();
        for (size_t i = 0; i < pillars.size(); ++i) {
            auto icon = std::make_unique<SpriteObject>();
            icon->Initialize(0);
            icon->GetMaterialData()->color = { 0.9f, 0.2f, 0.2f, 1.0f }; // 赤
            pillarIcons_.push_back(std::move(icon));
        }
    }

    // ズームカメラ視野枠
    for (int i = 0; i < 4; ++i) {
        zoomFrame2D_[i] = std::make_unique<SpriteObject>();
        zoomFrame2D_[i]->Initialize(0);
        zoomFrame2D_[i]->GetMaterialData()->color = { 0.0f, 0.8f, 0.0f, 1.0f }; // 緑
    }

    // ミニマップ外枠
    for (int i = 0; i < 4; ++i) {
        minimapBorderFrame2D_[i] = std::make_unique<SpriteObject>();
        minimapBorderFrame2D_[i]->Initialize(0);
        minimapBorderFrame2D_[i]->GetMaterialData()->color = { 0.5f, 0.5f, 0.5f, 1.0f }; // グレー
    }

    // 一括描画バッチ用GPUリソースの生成
    auto device = EngineResource::GetEngine()->GetDevice();

    // 頂点バッファ
    size_t maxVertices = kMaxMiniMapSegments * kVerticesPerSegment;
    batchVertexResource_ = DxUtils::CreateBufferResource(device, sizeof(VertexData) * maxVertices);
    batchVbView_.BufferLocation = batchVertexResource_->GetGPUVirtualAddress();
    batchVbView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * maxVertices);
    batchVbView_.StrideInBytes = sizeof(VertexData);

    // インデックスバッファ
    size_t maxIndices = kMaxMiniMapSegments * kIndicesPerSegment;
    batchIndexResource_ = DxUtils::CreateBufferResource(device, sizeof(uint32_t) * maxIndices);
    batchIbView_.BufferLocation = batchIndexResource_->GetGPUVirtualAddress();
    batchIbView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * maxIndices);
    batchIbView_.Format = DXGI_FORMAT_R32_UINT;

    // インデックス初期データの割り当て
    uint32_t* idxGPU = nullptr;
    batchIndexResource_->Map(0, nullptr, reinterpret_cast<void**>(&idxGPU));
    for (uint32_t i = 0; i < static_cast<uint32_t>(kMaxMiniMapSegments); ++i) {
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

    // マテリアル定数バッファ
    batchMaterialResource_ = DxUtils::CreateBufferResource(device, sizeof(Material));
    batchMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&batchMaterialData_));
    batchMaterialData_->color = { 0.0f, 0.0f, 0.0f, 1.0f }; // ルート軌跡線は黒
    batchMaterialData_->enableLighting = 0;
    batchMaterialData_->uvtransform = Math::MakeIdentity();
}

/**
 * @brief ミニマップの毎フレーム更新
 * @param input 入力マネージャ
 * @param route ルートデータ
 * @param stage ステージ情報
 * @param ioTargetZoom [in, out] ズームカメラの注視座標（ドラッグで更新・移動するため）
 */
void Minimap::Update(Input* input, Route* route, Stage* stage, Vector3& ioTargetZoom) {
    if (!route || !cameraMgr_) {
        cameraMgr_ = CameraResource::GetCameraManager();
    }
    if (!input || !cameraMgr_) return;

    float clientW = static_cast<float>(WindowApp::kClientWidth);
    float clientH = static_cast<float>(WindowApp::kClientHeight);

    Vector2 mousePos = GameViewWindow::GetMousePosition();
    Vector2 viewSize = GameViewWindow::GetGameViewSize();
    Vector2 scaledMousePos = mousePos;
    if (viewSize.x > 0.0f && viewSize.y > 0.0f) {
        scaledMousePos.x = (mousePos.x / viewSize.x) * clientW;
        scaledMousePos.y = (mousePos.y / viewSize.y) * clientH;
    }

    float vpWidth = clientW * kMinimapWidthRatio;
    float vpHeight = clientH;

    float aspect2D = vpWidth / vpHeight;

    float mapW = 0.0f;
    float mapH = 0.0f;
    if (aspect2D > kAspect3D) {
        mapH = vpHeight;
        mapW = mapH * kAspect3D;
    } else {
        mapW = vpWidth;
        mapH = mapW / kAspect3D;
    }

    float offsetX = (vpWidth - mapW) * 0.5f;
    float offsetY = (vpHeight - mapH) * 0.5f;

    float startZ = route->GetCurrentAreaStartZ();
    float goalZ = route->GetCurrentAreaGoalZ();

    // 1. ドラッグによるルート描画入力
    bool isClickStarted = input->MouseTrigger(0);
    bool isPressing = input->MousePress(0);

    if (!route->HasReachedGoal() && isPressing && (route->IsDrawing() || (isClickStarted && scaledMousePos.x <= vpWidth))) {
        float marginX = mapW * 0.1f;
        float tX = (scaledMousePos.x - (offsetX + marginX)) / (mapW - 2.0f * marginX);
        tX = std::clamp(tX, 0.0f, 1.0f);

        float marginY = mapH * 0.1f;
        float tZ = ((offsetY + mapH - marginY) - scaledMousePos.y) / (mapH - 2.0f * marginY);
        tZ = std::clamp(tZ, 0.0f, 1.0f);

        Vector3 dragWorldPos;
        dragWorldPos.x = -15.0f + tX * 30.0f;
        dragWorldPos.z = startZ + tZ * (goalZ - startZ);
        dragWorldPos.y = 0.0f;

        static Vector3 s_smoothedDragPos = dragWorldPos;
        if (isClickStarted) {
            s_smoothedDragPos = dragWorldPos;
        } else {
            s_smoothedDragPos.x = s_smoothedDragPos.x * 0.6f + dragWorldPos.x * 0.4f;
            s_smoothedDragPos.y = s_smoothedDragPos.y * 0.6f + dragWorldPos.y * 0.4f;
            s_smoothedDragPos.z = s_smoothedDragPos.z * 0.6f + dragWorldPos.z * 0.4f;
        }

        route->Update2D(s_smoothedDragPos);
        ioTargetZoom = s_smoothedDragPos;
    } else {
        route->Update2D({ 9999.0f, 0.0f, 0.0f });
    }

    // 2. 右クリックによるLoL風ミニマップ移動
    if (input->MousePress(1) && scaledMousePos.x <= vpWidth) {
        float marginX = mapW * 0.1f;
        float tX = (scaledMousePos.x - (offsetX + marginX)) / (mapW - 2.0f * marginX);
        tX = std::clamp(tX, 0.0f, 1.0f);

        float marginY = mapH * 0.1f;
        float tZ = ((offsetY + mapH - marginY) - scaledMousePos.y) / (mapH - 2.0f * marginY);
        tZ = std::clamp(tZ, 0.0f, 1.0f);

        ioTargetZoom.x = -15.0f + tX * 30.0f;
        ioTargetZoom.z = startZ + tZ * (goalZ - startZ);
    }

    // 3. 各2Dスプライトのパラメータ更新
    cameraMgr_->SetProjectionMatrix2D(Math::MakeOrthographicMatrix(0.0f, 0.0f, vpWidth, vpHeight, 0.0f, 100.0f));

    float marginX = mapW * 0.1f;
    float marginY = mapH * 0.1f;

    minimapBg_->SetSize(vpWidth, vpHeight);
    minimapBg_->SetPosition({ 0.0f, 0.0f });
    minimapBg_->Update();

    // スタート
    float startIconX = offsetX + marginX + 0.5f * (mapW - 2.0f * marginX);
    float startIconY = offsetY + mapH - marginY;
    startIcon_->SetSize(kStartGoalIconSize.x, kStartGoalIconSize.y);
    startIcon_->SetPosition({ startIconX - kStartGoalIconSize.x * 0.5f, startIconY - kStartGoalIconSize.y * 0.5f });
    startIcon_->Update();

    // ゴール
    float goalIconX = offsetX + marginX + 0.5f * (mapW - 2.0f * marginX);
    float goalIconY = offsetY + marginY;
    goalIcon_->SetSize(kStartGoalIconSize.x, kStartGoalIconSize.y);
    goalIcon_->SetPosition({ goalIconX - kStartGoalIconSize.x * 0.5f, goalIconY - kStartGoalIconSize.y * 0.5f });
    goalIcon_->Update();

    // ボス戦エリア用の赤四角の更新
    if (route->GetCurrentAreaIndex() == 3) {
        float tZ_boss = (kBossSpawnLineZ - startZ) / (goalZ - startZ);
        float pyBoss = (offsetY + mapH - marginY) - tZ_boss * (mapH - 2.0f * marginY);
        float pyGoal = offsetY + marginY; // Z = goalZ

        float bossAreaW = mapW - 2.0f * marginX;
        float bossAreaH = pyBoss - pyGoal;

        bossArea2D_->SetPosition({ offsetX + marginX, pyGoal });
        bossArea2D_->SetSize(bossAreaW, bossAreaH);
        bossArea2D_->Update();
    }

    // 柱
    if (stage) {
        const auto& pillars = stage->GetPillars();
        for (size_t i = 0; i < pillars.size() && i < pillarIcons_.size(); ++i) {
            Vector3 pPos = pillars[i]->GetPosition();
            float tX = (pPos.x - (-15.0f)) / 30.0f;
            float px = offsetX + marginX + tX * (mapW - 2.0f * marginX);

            float tZ = (pPos.z - startZ) / (goalZ - startZ);
            float py = (offsetY + mapH - marginY) - tZ * (mapH - 2.0f * marginY);

            pillarIcons_[i]->SetSize(kPillarIconSize.x, kPillarIconSize.y);
            pillarIcons_[i]->SetPosition({ px - kPillarIconSize.x * 0.5f, py - kPillarIconSize.y * 0.5f });
            pillarIcons_[i]->Update();
        }
    }

    // 自機
    float indTX = (ioTargetZoom.x - (-15.0f)) / 30.0f;
    float indPX = offsetX + marginX + indTX * (mapW - 2.0f * marginX);
    float indTZ = (ioTargetZoom.z - startZ) / (goalZ - startZ);
    float indPY = (offsetY + mapH - marginY) - indTZ * (mapH - 2.0f * marginY);
    indicatorIcon_->SetSize(kIndicatorIconSize.x, kIndicatorIconSize.y);
    indicatorIcon_->SetPosition({ indPX - kIndicatorIconSize.x * 0.5f, indPY - kIndicatorIconSize.y * 0.5f });
    indicatorIcon_->Update();

    // 手書き軌跡線の更新 (Catmull-Rom補間)
    const auto& rawPoints = route->GetRawPoints();
    activeMiniMapLineCount_ = 0;
    if (rawPoints.size() >= 2) {
        std::vector<Vector3> smoothedPoints = rawPoints;
        if (smoothedPoints.size() >= 3) {
            for (int iter = 0; iter < 3; ++iter) {
                std::vector<Vector3> temp = smoothedPoints;
                for (size_t i = 1; i < smoothedPoints.size() - 1; ++i) {
                    temp[i].x = (smoothedPoints[i - 1].x + smoothedPoints[i].x + smoothedPoints[i + 1].x) / 3.0f;
                    temp[i].y = (smoothedPoints[i - 1].y + smoothedPoints[i].y + smoothedPoints[i + 1].y) / 3.0f;
                    temp[i].z = (smoothedPoints[i - 1].z + smoothedPoints[i].z + smoothedPoints[i + 1].z) / 3.0f;
                }
                smoothedPoints = temp;
            }
        }

        std::vector<Vector3> densePoints = Math::GenerateCatmullRomPath(smoothedPoints, 5);

        size_t neededLines = 0;
        if (densePoints.size() >= 2) {
            neededLines = densePoints.size() - 1;
        }

        if (neededLines > kMaxMiniMapSegments) {
            neededLines = kMaxMiniMapSegments;
        }

        activeMiniMapLineCount_ = neededLines;

        if (neededLines > 0) {
            VertexData* vtx = nullptr;
            batchVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));

            float halfThickness = kRouteLineThickness * 0.5f;

            for (size_t i = 0; i < neededLines; ++i) {
                Vector3 pt0 = densePoints[i];
                Vector3 pt1 = densePoints[i + 1];

                float tX0 = (pt0.x - (-15.0f)) / 30.0f;
                float px0 = offsetX + marginX + tX0 * (mapW - 2.0f * marginX);
                float tZ0 = (pt0.z - startZ) / (goalZ - startZ);
                float py0 = (offsetY + mapH - marginY) - tZ0 * (mapH - 2.0f * marginY);

                float tX1 = (pt1.x - (-15.0f)) / 30.0f;
                float px1 = offsetX + marginX + tX1 * (mapW - 2.0f * marginX);
                float tZ1 = (pt1.z - startZ) / (goalZ - startZ);
                float py1 = (offsetY + mapH - marginY) - tZ1 * (mapH - 2.0f * marginY);

                float dx = px1 - px0;
                float dy = py1 - py0;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 0.001f) {
                    dist = 0.001f;
                }

                float angle = std::atan2(dy, dx);
                float cos_t = std::cos(angle);
                float sin_t = std::sin(angle);

                size_t baseVtx = i * 4;

                // 頂点0 (左下)
                vtx[baseVtx + 0].position = { px0 - halfThickness * sin_t, py0 + halfThickness * cos_t, 0.0f, 1.0f };
                vtx[baseVtx + 0].normal = { 0.0f, 0.0f, -1.0f };
                vtx[baseVtx + 0].texcoord = { 0.0f, 1.0f };

                // 頂点1 (左上)
                vtx[baseVtx + 1].position = { px0 + halfThickness * sin_t, py0 - halfThickness * cos_t, 0.0f, 1.0f };
                vtx[baseVtx + 1].normal = { 0.0f, 0.0f, -1.0f };
                vtx[baseVtx + 1].texcoord = { 0.0f, 0.0f };

                // 頂点2 (右下)
                vtx[baseVtx + 2].position = { px0 + dist * cos_t - halfThickness * sin_t, py0 + dist * sin_t + halfThickness * cos_t, 0.0f, 1.0f };
                vtx[baseVtx + 2].normal = { 0.0f, 0.0f, -1.0f };
                vtx[baseVtx + 2].texcoord = { 1.0f, 1.0f };

                // 頂点3 (右上)
                vtx[baseVtx + 3].position = { px0 + dist * cos_t + halfThickness * sin_t, py0 + dist * sin_t - halfThickness * cos_t, 0.0f, 1.0f };
                vtx[baseVtx + 3].normal = { 0.0f, 0.0f, -1.0f };
                vtx[baseVtx + 3].texcoord = { 1.0f, 0.0f };
            }

            batchVertexResource_->Unmap(0, nullptr);
        }
    }

    // カメラ視野範囲枠 (2D)
    static const float kZoomViewWidth3D = 24.0f;
    static const float kZoomViewHeight3D = 18.0f;

    float vpRightAspect = (clientW * 0.7f) / clientH;
    float viewH3D = kZoomViewHeight3D;
    float viewW3D = viewH3D * vpRightAspect;

    float minX3D = ioTargetZoom.x - viewW3D * 0.5f;
    float maxX3D = ioTargetZoom.x + viewW3D * 0.5f;
    float minZ3D = ioTargetZoom.z - viewH3D * 0.5f;
    float maxZ3D = ioTargetZoom.z + viewH3D * 0.5f;

    auto to2D = [&](float wx, float wz) -> Vector2 {
        float tX = (wx - (-15.0f)) / 30.0f;
        float px = offsetX + marginX + tX * (mapW - 2.0f * marginX);

        float tZ = (wz - startZ) / (goalZ - startZ);
        float py = (offsetY + mapH - marginY) - tZ * (mapH - 2.0f * marginY);
        return { px, py };
    };

    Vector2 pMin = to2D(minX3D, minZ3D);
    Vector2 pMax = to2D(maxX3D, maxZ3D);

    float x0 = pMin.x;
    float x1 = pMax.x;
    float y0 = pMax.y;
    float y1 = pMin.y;

    zoomFrame2D_[0]->SetPosition({ x0, y0 });
    zoomFrame2D_[0]->SetSize(x1 - x0, kFrameThickness);
    zoomFrame2D_[0]->Update();

    zoomFrame2D_[1]->SetPosition({ x0, y1 - kFrameThickness });
    zoomFrame2D_[1]->SetSize(x1 - x0, kFrameThickness);
    zoomFrame2D_[1]->Update();

    zoomFrame2D_[2]->SetPosition({ x0, y0 });
    zoomFrame2D_[2]->SetSize(kFrameThickness, y1 - y0);
    zoomFrame2D_[2]->Update();

    zoomFrame2D_[3]->SetPosition({ x1 - kFrameThickness, y0 });
    zoomFrame2D_[3]->SetSize(kFrameThickness, y1 - y0);
    zoomFrame2D_[3]->Update();

    // ミニマップ境界外枠 (2D)
    float bx0 = offsetX + marginX;
    float bx1 = offsetX + mapW - marginX;
    float by0 = offsetY + marginY;
    float by1 = offsetY + mapH - marginY;

    minimapBorderFrame2D_[0]->SetPosition({ bx0, by0 });
    minimapBorderFrame2D_[0]->SetSize(bx1 - bx0, kFrameThickness);
    minimapBorderFrame2D_[0]->Update();

    minimapBorderFrame2D_[1]->SetPosition({ bx0, by1 - kFrameThickness });
    minimapBorderFrame2D_[1]->SetSize(bx1 - bx0, kFrameThickness);
    minimapBorderFrame2D_[1]->Update();

    minimapBorderFrame2D_[2]->SetPosition({ bx0, by0 });
    minimapBorderFrame2D_[2]->SetSize(kFrameThickness, by1 - by0);
    minimapBorderFrame2D_[2]->Update();

    minimapBorderFrame2D_[3]->SetPosition({ bx1 - kFrameThickness, by0 });
    minimapBorderFrame2D_[3]->SetSize(kFrameThickness, by1 - by0);
    minimapBorderFrame2D_[3]->Update();

    cameraMgr_->SetProjectionMatrix2D(Math::MakeOrthographicMatrix(0.0f, 0.0f, clientW, clientH, 0.0f, 100.0f));
}

void Minimap::Draw(int currentAreaIndex) {
    minimapBg_->Draw("white");

    // ボスエリアは背景のすぐ上（背後）に描画
    if (currentAreaIndex == 3 && bossArea2D_) {
        bossArea2D_->Draw("white");
    }

    for (auto& icon : pillarIcons_) {
        icon->Draw("white");
    }

    startIcon_->Draw("circle_solid");
    goalIcon_->Draw("circle_solid");

    if (activeMiniMapLineCount_ > 0) {
        auto commandList = EngineResource::GetEngine()->GetDxCommon()->GetCommandList();

        float clientW = static_cast<float>(WindowApp::kClientWidth);
        float clientH = static_cast<float>(WindowApp::kClientHeight);
        float vpWidth = clientW * kMinimapWidthRatio;
        float vpHeight = clientH;

        // WVP行列の更新 (単位行列 * View2D * 正しいProj2D)
        Matrix4x4 viewMat = cameraMgr_->GetViewMatrix2D();
        Matrix4x4 projMat = Math::MakeOrthographicMatrix(0.0f, 0.0f, vpWidth, vpHeight, 0.0f, 100.0f);
        Matrix4x4 wvp = Math::Multiply(Math::Multiply(Math::MakeIdentity(), viewMat), projMat);
        batchWvpData_->WVP = wvp;
        batchWvpData_->world = Math::MakeIdentity();

        commandList->SetGraphicsRootSignature(EngineResource::GetEngine()->GetPSOManager()->GetRootSignature("Object2D"));
        commandList->SetPipelineState(EngineResource::GetEngine()->GetPSOManager()->GetPSO("Object2D"));

        commandList->IASetVertexBuffers(0, 1, &batchVbView_);
        commandList->IASetIndexBuffer(&batchIbView_);
        commandList->SetGraphicsRootConstantBufferView(0, batchWvpResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, batchMaterialResource_->GetGPUVirtualAddress());

        auto texMgr = TextureResource::GetTextureManager();
        if (texMgr) {
            commandList->SetGraphicsRootDescriptorTable(2, texMgr->GetGpuHandle("white"));
        }

        commandList->DrawIndexedInstanced(static_cast<UINT>(activeMiniMapLineCount_ * kIndicesPerSegment), 1, 0, 0, 0);
    }

    indicatorIcon_->Draw("circle_solid");

    for (int i = 0; i < 4; ++i) {
        zoomFrame2D_[i]->Draw("white");
    }

    for (int i = 0; i < 4; ++i) {
        minimapBorderFrame2D_[i]->Draw("white");
    }
}
