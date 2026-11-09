#ifdef _USEIMGUI
#include "Engine/Debug/MapStudioWindow.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Input/Input.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <windows.h>
#include <commdlg.h>

namespace {
    const std::string kStudioCameraName = "MapStudioCamera";

    // 背景色プリセット定数 (マジックナンバー排除)
    constexpr Vector4 kBgColorDarkGray  = { 0.12f, 0.12f, 0.14f, 1.0f };
    constexpr Vector4 kBgColorPureBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr Vector4 kBgColorBlue      = { 0.08f, 0.14f, 0.25f, 1.0f };

    constexpr const char* kBgColorNames[] = {
        "ダークグレー (標準)",
        "漆黒 (ブラック)",
        "ディープブルー"
    };

    // カメラ定数
    constexpr float kFovY = 45.0f * (3.14159265f / 180.0f);
    constexpr float kNearZ = 0.5f;
    constexpr float kFarZ = 2000.0f;
    constexpr float kHalfFactor = 0.5f;
    constexpr float kControlBarHeight = 44.0f;
    constexpr float kGroundPlateThickness = 0.2f;
    constexpr float kGroundPlateYOffset = -0.1f;
    constexpr float kPreviewHoverScaleBoost = 1.03f;
    constexpr float kRaycastEpsilon = 1e-5f;

    // グリッド線・境界線定数 (マジックナンバー排除)
    constexpr float kGridLineY = 0.02f;           // 地面からわずかに浮かせてZファイティングを防止
    constexpr float kBorderLineY = 0.03f;         // 境界枠の高さ
    constexpr float kHoverBorderY = 0.04f;        // ホバー枠の高さ
    constexpr float kGridLineThickness = 0.04f;   // 通常グリッド線幅
    constexpr float kMajorGridLineThickness = 0.08f; // 5マスごとの主グリッド線幅
    constexpr float kBorderLineThickness = 0.16f; // ステージ外枠境界線幅
    constexpr float kHoverBorderThickness = 0.12f;// 接地ホバー枠幅
    constexpr int   kMajorGridInterval = 5;       // 主線の間隔 (5マスごと)

    // カラーパレット定数
    constexpr Vector4 kGroundPlateColor        = { 0.14f, 0.16f, 0.20f, 1.0f }; // 地面ベースプレート
    constexpr Vector4 kGridLineColor           = { 0.32f, 0.38f, 0.48f, 0.55f }; // 通常グリッド線
    constexpr Vector4 kGridMajorLineColor      = { 0.55f, 0.70f, 0.90f, 0.85f }; // 5マスごとの主グリッド線
    constexpr Vector4 kStageBorderColor        = { 0.20f, 0.75f, 1.00f, 1.0f };  // ステージ外枠境界 (シアン)
    constexpr Vector4 kStartLineColor          = { 0.20f, 1.00f, 0.45f, 1.0f };  // START手前ライン (グリーン)
    constexpr Vector4 kGoalLineColor           = { 1.00f, 0.85f, 0.20f, 1.0f };  // GOAL奥ライン (ゴールド)
    constexpr Vector4 kHoverBuildColor         = { 0.25f, 1.00f, 0.45f, 0.40f }; // ホバー半透明キューブ (配置)
    constexpr Vector4 kHoverClearColor         = { 1.00f, 0.25f, 0.25f, 0.40f }; // ホバー半透明キューブ (消去)
    constexpr Vector4 kHoverBorderBuildColor   = { 0.30f, 1.00f, 0.55f, 1.0f };  // 接地ホバー枠 (配置)
    constexpr Vector4 kHoverBorderClearColor   = { 1.00f, 0.30f, 0.30f, 1.0f };  // 接地ホバー枠 (消去)

    // ワイド文字列変換
    std::string WideToUTF8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int sizeNeeded = ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        if (sizeNeeded <= 0) return "";
        std::string strTo(sizeNeeded, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, nullptr, nullptr);
        return strTo;
    }

    std::wstring UTF8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int sizeNeeded = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
        if (sizeNeeded <= 0) return L"";
        std::wstring wstrTo(sizeNeeded, 0);
        ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
        return wstrTo;
    }

    // 3Dライン描画ヘルパー
    void DrawLineSegment(LineObject* lineObj, const Vector3& start, const Vector3& end, const Vector4& color, float thickness) {
        if (!lineObj) return;
        lineObj->SetStartPoint(start);
        lineObj->SetEndPoint(end);
        lineObj->SetThickness(thickness);
        lineObj->GetMaterialData()->color = color;
        lineObj->Update();
        lineObj->Draw("white");
    }
}

MapStudioWindow::MapStudioWindow() {
    studioCamera_ = std::make_shared<BaseCamera>();
    studioCamera_->Initialize();
    ResetCamera();
}

MapStudioWindow::~MapStudioWindow() = default;

void MapStudioWindow::ResetCamera() {
    float totalH = static_cast<float>(mapGrid_.height) * mapGrid_.tileSize;
    cameraPosition_ = { 0.0f, (std::max)(30.0f, totalH * 0.45f), -(std::max)(35.0f, totalH * 0.65f) };
    cameraRotation_ = { 0.62f, 0.0f, 0.0f }; // 俯瞰約35度
    if (studioCamera_) {
        studioCamera_->SetPosition(cameraPosition_);
        studioCamera_->SetRotation(cameraRotation_);
        studioCamera_->Update();
    }
}

void MapStudioWindow::SetTopDownView() {
    float totalH = static_cast<float>(mapGrid_.height) * mapGrid_.tileSize;
    cameraPosition_ = { 0.0f, (std::max)(50.0f, totalH * 0.95f), 0.0f };
    cameraRotation_ = { 1.54f, 0.0f, 0.0f }; // ほぼ真上見下ろし (90度)
    if (studioCamera_) {
        studioCamera_->SetPosition(cameraPosition_);
        studioCamera_->SetRotation(cameraRotation_);
        studioCamera_->Update();
    }
}

void MapStudioWindow::UpdateCamera() {
    if (!studioCamera_) return;

    ImGuiIO& io = ImGui::GetIO();

    // Viewportがホバーされており、右クリックが押されている場合はデバッグカメラ操作
    if (isViewportHovered_ && io.MouseDown[1]) {
        isCameraNavigating_ = true;

        // 1. マウス移動による視線回転 (ピッチ・ヨー)
        cameraRotation_.y += io.MouseDelta.x * kCameraRotateSpeed;
        cameraRotation_.x += io.MouseDelta.y * kCameraRotateSpeed;
        cameraRotation_.x = std::clamp(cameraRotation_.x, -kMaxCameraPitch, kMaxCameraPitch);

        // 2. 移動方向ベクトルの算出 (回転行列から forward, right, up を取得)
        Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);
        Vector3 forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
        Vector3 right   = Math::TransformNormal({ 1.0f, 0.0f, 0.0f }, rotateMatrix);
        Vector3 up      = { 0.0f, 1.0f, 0.0f };

        Vector3 move = { 0.0f, 0.0f, 0.0f };
        if (ImGui::IsKeyDown(ImGuiKey_W)) move = move + forward;
        if (ImGui::IsKeyDown(ImGuiKey_S)) move = move - forward;
        if (ImGui::IsKeyDown(ImGuiKey_D)) move = move + right;
        if (ImGui::IsKeyDown(ImGuiKey_A)) move = move - right;
        if (ImGui::IsKeyDown(ImGuiKey_Space) || ImGui::IsKeyDown(ImGuiKey_E)) move = move + up;
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_Q)) move = move - up;

        if (Math::Length(move) > 0.0f) {
            float speed = kCameraMoveSpeed;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                speed *= kCameraFastMultiplier;
            }
            move = Math::Normalize(move) * speed;
            cameraPosition_ = cameraPosition_ + move;
        }
    } else {
        isCameraNavigating_ = false;

        // Viewport上のマウスホイールで前後ドリー移動
        if (isViewportHovered_ && std::abs(io.MouseWheel) > 0.01f) {
            Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(cameraRotation_.x, cameraRotation_.y, cameraRotation_.z);
            Vector3 forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
            cameraPosition_ = cameraPosition_ + forward * (io.MouseWheel * kCameraWheelSensitivity);
        }
    }

    studioCamera_->SetPosition(cameraPosition_);
    studioCamera_->SetRotation(cameraRotation_);
    if (viewportSize_.y > 0.0f) {
        studioCamera_->UpdateProjection(viewportSize_.x / viewportSize_.y);
    }
    studioCamera_->Update();
}

bool MapStudioWindow::PerformGroundRaycast(const Vector2& mouseLocalPos, int& outGx, int& outGz) {
    if (!studioCamera_ || viewportSize_.x <= 0.0f || viewportSize_.y <= 0.0f) return false;

    Vector3 rayStart{}, rayDir{};
    studioCamera_->CreateRay(mouseLocalPos, viewportSize_.x, viewportSize_.y, rayStart, rayDir);

    if (std::abs(rayDir.y) < kRaycastEpsilon) return false;

    // 地面 Y = 0 平面との交点 (t = -rayStart.y / rayDir.y)
    float t = -rayStart.y / rayDir.y;
    if (t <= 0.0f) return false;

    Vector3 hitPos = {
        rayStart.x + t * rayDir.x,
        0.0f,
        rayStart.z + t * rayDir.z
    };

    const float tileSize = mapGrid_.tileSize;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float startX = -totalWidth * kHalfFactor;
    const float startZ = -totalHeight * kHalfFactor;

    int gx = static_cast<int>(std::floor((hitPos.x - startX) / tileSize));
    int gz = static_cast<int>(std::floor((hitPos.z - startZ) / tileSize));

    if (gx >= 0 && gx < mapGrid_.width && gz >= 0 && gz < mapGrid_.height) {
        outGx = gx;
        outGz = gz;
        return true;
    }
    return false;
}

bool MapStudioWindow::Initialize() {
    if (isInitialized_) return true;

    auto engine = EngineResource::GetEngine();
    if (!engine) return false;
    ID3D12Device* device = engine->GetDevice();
    auto dxCommon = engine->GetDxCommon();
    if (!device || !dxCommon) return false;

    // 1. プレビュー専用 RTV ヒープを作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)))) {
        return false;
    }
    rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    // 2. グローバル SRV ヒープから専用スロット 5 を取得
    ID3D12DescriptorHeap* srvHeap = dxCommon->GetSrvHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    srvCpuHandle_ = DxUtils::GetCPUDescriptorHandle(srvHeap, descriptorSize, kStudioSrvHeapIndex);
    srvGpuHandle_ = DxUtils::GetGPUDescriptorHandle(srvHeap, descriptorSize, kStudioSrvHeapIndex);

    // 3. レンダーテクスチャの生成
    previewRenderTexture_ = std::make_unique<RenderTexture>();
    previewRenderTexture_->Initialize(
        device,
        kDefaultPreviewWidth,
        kDefaultPreviewHeight,
        dxCommon->GetRtvFormat(),
        kBgColorDarkGray,
        rtvHandle_,
        srvCpuHandle_,
        srvGpuHandle_,
        L"MapStudioPreview"
    );

    // 4. プレビュー専用深度ステンシル
    depthStencilResource_ = DxUtils::CreateDepthStencilTextureResource(device, kDefaultPreviewWidth, kDefaultPreviewHeight);
    dsvHeap_ = DxUtils::CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);

    // 5. 描画用プリミティブ
    previewCube_ = std::make_unique<CubeObject>();
    previewCube_->Initialize(2);

    previewLine_ = std::make_unique<LineObject>();
    previewLine_->Initialize(0);

    // 6. スタジオ専用カメラを CameraManager に登録
    if (!studioCamera_) {
        studioCamera_ = std::make_shared<BaseCamera>();
        studioCamera_->Initialize();
    }
    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr && !cameraMgr->HasCamera(kStudioCameraName)) {
        cameraMgr->AddCamera(kStudioCameraName, studioCamera_);
    }

    ResetCamera();
    isInitialized_ = true;
    return true;
}

void MapStudioWindow::RenderPreview(ID3D12GraphicsCommandList* cmdList) {
    if (!isInitialized_ || !cmdList || !previewRenderTexture_ || !studioCamera_) return;

    auto engine = EngineResource::GetEngine();
    if (!engine) return;
    auto dxCommon = engine->GetDxCommon();
    if (!dxCommon) return;

    // ★重要 [クラッシュ防止]: SRV ディスクリプタヒープをコマンドリストへ確実にバインド
    ID3D12DescriptorHeap* heaps[] = { dxCommon->GetSrvHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // ★重要: スタジオカメラを一時的にアクティブ化し、定数バッファをGPU更新
    auto cameraMgr = CameraResource::GetCameraManager();
    std::string prevActiveCam = "";
    if (cameraMgr) {
        prevActiveCam = cameraMgr->GetActiveCameraName();
        cameraMgr->SetActiveCamera(kStudioCameraName, false);
        cameraMgr->Update();
    }

    Vector4 clearColor = kBgColorDarkGray;
    switch (selectedBgColorIndex_) {
        case 1: clearColor = kBgColorPureBlack; break;
        case 2: clearColor = kBgColorBlue;      break;
        default: break;
    }

    // 1. レンダーターゲット設定
    previewRenderTexture_->PreDraw(cmdList, dsvHandle_, true);

    // 2. ビューポート設定
    D3D12_VIEWPORT vp{};
    vp.Width = static_cast<float>(kDefaultPreviewWidth);
    vp.Height = static_cast<float>(kDefaultPreviewHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &vp);

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(kDefaultPreviewWidth);
    scissor.bottom = static_cast<LONG>(kDefaultPreviewHeight);
    cmdList->RSSetScissorRects(1, &scissor);

    // 3. 3D空間オブジェクト描画
    const float tileSize = mapGrid_.tileSize;
    const float buildingH = mapGrid_.buildingHeight;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float startX = -totalWidth * kHalfFactor;
    const float startZ = -totalHeight * kHalfFactor;

    if (previewCube_) {
        // (A) 地面ベースプレート
        previewCube_->SetPosition({ 0.0f, kGroundPlateYOffset, 0.0f });
        previewCube_->SetScale({ totalWidth, kGroundPlateThickness, totalHeight });
        previewCube_->SetRotate({ 0.0f, 0.0f, 0.0f });
        previewCube_->GetMaterialData()->color = kGroundPlateColor;
        previewCube_->Update();
        previewCube_->Draw("white");

        // (B) 配置済みビルの3D描画
        for (int z = 0; z < mapGrid_.height; ++z) {
            for (int x = 0; x < mapGrid_.width; ++x) {
                if (mapGrid_.GetTile(x, z) != kTileBuilding) continue;

                float posX = startX + (static_cast<float>(x) + kHalfFactor) * tileSize;
                float posZ = startZ + (static_cast<float>(z) + kHalfFactor) * tileSize;
                float posY = buildingH * kHalfFactor;

                previewCube_->SetPosition({ posX, posY, posZ });
                previewCube_->SetScale({ tileSize, buildingH, tileSize });
                previewCube_->SetRotate({ 0.0f, 0.0f, 0.0f });
                previewCube_->GetMaterialData()->color = mapGrid_.buildingColor;
                previewCube_->Update();
                previewCube_->Draw("white");
            }
        }

        // (C) 3D空間ホバープレビュー (立体的な半透明キューブ)
        if (isHoveredValid_ && hoveredGridX_ >= 0 && hoveredGridZ_ >= 0) {
            float hPosX = startX + (static_cast<float>(hoveredGridX_) + kHalfFactor) * tileSize;
            float hPosZ = startZ + (static_cast<float>(hoveredGridZ_) + kHalfFactor) * tileSize;
            float hPosY = buildingH * kHalfFactor;

            previewCube_->SetPosition({ hPosX, hPosY, hPosZ });
            previewCube_->SetScale({ tileSize * kPreviewHoverScaleBoost, buildingH * kPreviewHoverScaleBoost, tileSize * kPreviewHoverScaleBoost });
            previewCube_->SetRotate({ 0.0f, 0.0f, 0.0f });
            previewCube_->GetMaterialData()->color = (activeTool_ == kTileBuilding) ? kHoverBuildColor : kHoverClearColor;
            previewCube_->Update();
            previewCube_->Draw("white");
        }
    }

    if (previewLine_) {
        // (D) 3D地面グリッド線（格子ライン）の描画
        // X方向に平行な横線 (Z軸方向の各マス)
        for (int z = 0; z <= mapGrid_.height; ++z) {
            float pz = startZ + static_cast<float>(z) * tileSize;
            bool isMajor = (z % kMajorGridInterval == 0);
            Vector4 col = isMajor ? kGridMajorLineColor : kGridLineColor;
            float th = isMajor ? kMajorGridLineThickness : kGridLineThickness;
            DrawLineSegment(previewLine_.get(), { startX, kGridLineY, pz }, { startX + totalWidth, kGridLineY, pz }, col, th);
        }

        // Z方向に平行な縦線 (X軸方向の各マス)
        for (int x = 0; x <= mapGrid_.width; ++x) {
            float px = startX + static_cast<float>(x) * tileSize;
            bool isMajor = (x % kMajorGridInterval == 0);
            Vector4 col = isMajor ? kGridMajorLineColor : kGridLineColor;
            float th = isMajor ? kMajorGridLineThickness : kGridLineThickness;
            DrawLineSegment(previewLine_.get(), { px, kGridLineY, startZ }, { px, kGridLineY, startZ + totalHeight }, col, th);
        }

        // (E) ステージ外周境界線 (太枠)
        // 左右の境界線
        DrawLineSegment(previewLine_.get(), { startX, kBorderLineY, startZ }, { startX, kBorderLineY, startZ + totalHeight }, kStageBorderColor, kBorderLineThickness);
        DrawLineSegment(previewLine_.get(), { startX + totalWidth, kBorderLineY, startZ }, { startX + totalWidth, kBorderLineY, startZ + totalHeight }, kStageBorderColor, kBorderLineThickness);

        // START (手前側境界ライン: エメラルドグリーン)
        DrawLineSegment(previewLine_.get(), { startX, kBorderLineY, startZ }, { startX + totalWidth, kBorderLineY, startZ }, kStartLineColor, kBorderLineThickness * 1.5f);

        // GOAL (奥側境界ライン: ゴールドイエロー)
        DrawLineSegment(previewLine_.get(), { startX, kBorderLineY, startZ + totalHeight }, { startX + totalWidth, kBorderLineY, startZ + totalHeight }, kGoalLineColor, kBorderLineThickness * 1.5f);

        // (F) 接地ホバー枠 (ホバー中マスの地面四辺ハイライト)
        if (isHoveredValid_ && hoveredGridX_ >= 0 && hoveredGridZ_ >= 0) {
            float hx0 = startX + static_cast<float>(hoveredGridX_) * tileSize;
            float hx1 = hx0 + tileSize;
            float hz0 = startZ + static_cast<float>(hoveredGridZ_) * tileSize;
            float hz1 = hz0 + tileSize;

            Vector4 hBorderCol = (activeTool_ == kTileBuilding) ? kHoverBorderBuildColor : kHoverBorderClearColor;
            DrawLineSegment(previewLine_.get(), { hx0, kHoverBorderY, hz0 }, { hx1, kHoverBorderY, hz0 }, hBorderCol, kHoverBorderThickness);
            DrawLineSegment(previewLine_.get(), { hx1, kHoverBorderY, hz0 }, { hx1, kHoverBorderY, hz1 }, hBorderCol, kHoverBorderThickness);
            DrawLineSegment(previewLine_.get(), { hx1, kHoverBorderY, hz1 }, { hx0, kHoverBorderY, hz1 }, hBorderCol, kHoverBorderThickness);
            DrawLineSegment(previewLine_.get(), { hx0, kHoverBorderY, hz1 }, { hx0, kHoverBorderY, hz0 }, hBorderCol, kHoverBorderThickness);
        }
    }

    // 4. レンダーテクスチャをSRV状態へ遷移
    previewRenderTexture_->PostDraw(cmdList);

    // ★重要: メインシーンのカメラへ復元
    if (cameraMgr && !prevActiveCam.empty()) {
        cameraMgr->SetActiveCamera(prevActiveCam, false);
        cameraMgr->Update();
    }
}

void MapStudioWindow::Draw(bool* open, ID3D12GraphicsCommandList* cmdList) {
    if (!open || !(*open)) {
        isOpen_ = false;
        return;
    }
    isOpen_ = true;

    if (!isInitialized_) {
        Initialize();
    }

    UpdateCamera();
    RenderPreview(cmdList);

    // メインスタジオウィンドウ (エフェクトエディター配置)
    ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Map Studio (3Dマップエディター)###MapStudio", open, ImGuiWindowFlags_MenuBar)) {
        DrawToolbar();

        float availableWidth = ImGui::GetContentRegionAvail().x;
        float maxInspectorWidth = (std::max)(kMinInspectorWidth, availableWidth - kMinViewportWidth - kSplitterWidth);
        inspectorWidth_ = std::clamp(inspectorWidth_, kMinInspectorWidth, maxInspectorWidth);
        float leftWidth = (std::max)(kMinViewportWidth, availableWidth - inspectorWidth_ - kSplitterWidth);

        // 左ペイン (メインビューポート + 下部操作バー)
        ImGui::BeginChild("ViewportArea", ImVec2(leftWidth, 0), false);
        {
            DrawViewport();
            DrawControlBar();
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);

        // 境界スプリッター
        constexpr ImVec4 kSplitterColorNormal  = { 0.20f, 0.21f, 0.24f, 0.6f };
        constexpr ImVec4 kSplitterColorHovered = { 0.35f, 0.60f, 0.95f, 0.9f };
        constexpr ImVec4 kSplitterColorActive  = { 0.25f, 0.70f, 1.00f, 1.0f };

        ImGui::PushStyleColor(ImGuiCol_Button, kSplitterColorNormal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSplitterColorHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSplitterColorActive);
        ImGui::Button("##vsplitter", ImVec2(kSplitterWidth, -1.0f));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            float mouseDeltaX = ImGui::GetIO().MouseDelta.x;
            inspectorWidth_ -= mouseDeltaX;
            inspectorWidth_ = std::clamp(inspectorWidth_, kMinInspectorWidth, maxInspectorWidth);
        }

        ImGui::SameLine(0.0f, 0.0f);

        // 右ペイン (スタジオ・インスペクター)
        ImGui::BeginChild("InspectorArea", ImVec2(0, 0), true);
        {
            DrawStudioInspector();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void MapStudioWindow::DrawToolbar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("カメラリセット")) {
            ResetCamera();
        }
        if (ImGui::MenuItem("見下ろし視点 (Top-down)")) {
            SetTopDownView();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("元に戻す (Ctrl+Z)", nullptr, false, !undoStack_.empty())) {
            ExecuteUndo();
        }
        if (ImGui::MenuItem("やり直す (Ctrl+Y)", nullptr, false, !redoStack_.empty())) {
            ExecuteRedo();
        }
        ImGui::EndMenuBar();
    }

    // 上部ツールバー行
    ImGui::Text("マップ名:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::InputText("##MapName", mapNameBuffer_, sizeof(mapNameBuffer_))) {
        currentMapName_ = mapNameBuffer_;
    }

    ImGui::SameLine();
    if (ImGui::Button("JSON保存")) {
        SaveMapAsset("resources/Maps/" + currentMapName_ + ".json");
    }

    ImGui::SameLine();
    if (ImGui::Button("JSON読込")) {
        OpenMapAsset("resources/Maps/" + currentMapName_ + ".json");
    }

    ImGui::SameLine();
    size_t buildingCount = mapGrid_.GetBuildingCount();
    ImGui::TextDisabled(" | 配置ビル数: %zu 棟 (グリッド: %d x %d)", buildingCount, mapGrid_.width, mapGrid_.height);

    ImGui::Separator();
}

void MapStudioWindow::DrawViewport() {
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    float vpHeight = (std::max)(150.0f, contentSize.y - kControlBarHeight);
    float vpWidth = contentSize.x;
    viewportSize_ = { vpWidth, vpHeight };

    // プレビューテクスチャを描画
    ImTextureID texID = (ImTextureID)srvGpuHandle_.ptr;
    viewportScreenPos_ = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };
    ImGui::Image(texID, ImVec2(vpWidth, vpHeight));

    isViewportHovered_ = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    // 1. 3D地面レイキャスト (右ドラッグ操作中以外で判定)
    if (isViewportHovered_ && !io.MouseDown[1]) {
        Vector2 mouseLocal = { io.MousePos.x - viewportScreenPos_.x, io.MousePos.y - viewportScreenPos_.y };
        isHoveredValid_ = PerformGroundRaycast(mouseLocal, hoveredGridX_, hoveredGridZ_);
    } else {
        isHoveredValid_ = false;
    }

    // 2. 左クリック/ドラッグによる 3D マス目へのビル配置または消去
    if (isViewportHovered_ && !io.MouseDown[1]) {
        if (io.MouseDown[0] && isHoveredValid_) {
            if (hoveredGridX_ != lastEditedGridX_ || hoveredGridZ_ != lastEditedGridZ_) {
                if (activeTool_ == kTileBuilding) {
                    PlaceBuilding(hoveredGridX_, hoveredGridZ_);
                } else {
                    RemoveBuilding(hoveredGridX_, hoveredGridZ_);
                }
                lastEditedGridX_ = hoveredGridX_;
                lastEditedGridZ_ = hoveredGridZ_;
            }
        } else if (!io.MouseDown[0]) {
            lastEditedGridX_ = -1;
            lastEditedGridZ_ = -1;
            if (!currentBatch_.empty()) {
                UndoStep step;
                step.actions = std::move(currentBatch_);
                undoStack_.push_back(std::move(step));
                if (undoStack_.size() > kMaxUndoHistory) {
                    undoStack_.erase(undoStack_.begin());
                }
                currentBatch_.clear();
                redoStack_.clear();
            }
        }
    }

    // 3. 画面内オーバーレイUI (デバッグカメラ操作ガイド・ステージ範囲情報・ホバー座標)
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (drawList && vpWidth > 0.0f && vpHeight > 0.0f) {
        // スタイル定数
        constexpr ImU32 kBannerBg = IM_COL32(16, 20, 28, 200);
        constexpr ImU32 kBannerBorder = IM_COL32(50, 70, 95, 220);
        constexpr ImU32 kTextColor = IM_COL32(230, 235, 245, 240);
        constexpr ImU32 kSubTextColor = IM_COL32(160, 190, 220, 230);
        constexpr ImU32 kHighlightColor = IM_COL32(80, 200, 255, 255);
        constexpr ImU32 kStartLineBadgeColor = IM_COL32(50, 230, 100, 255);
        constexpr ImU32 kGoalLineBadgeColor = IM_COL32(255, 210, 50, 255);

        // (A) 操作ガイドバナー (左上 1段目)
        ImVec2 banner1Min = ImVec2(viewportScreenPos_.x + 10.0f, viewportScreenPos_.y + 10.0f);
        ImVec2 banner1Max = ImVec2(viewportScreenPos_.x + 480.0f, viewportScreenPos_.y + 34.0f);
        drawList->AddRectFilled(banner1Min, banner1Max, kBannerBg, 4.0f);
        drawList->AddRect(banner1Min, banner1Max, kBannerBorder, 4.0f);

        std::string guideText = "右ドラッグ: 視点回転 | WASD: 移動 | Shift: ダッシュ | 左クリック: ";
        guideText += (activeTool_ == kTileBuilding) ? "ビル配置" : "空白消去";
        drawList->AddText(ImVec2(banner1Min.x + 8.0f, banner1Min.y + 4.0f), kTextColor, guideText.c_str());

        // (B) ステージ範囲・進行方向バナー (左上 2段目)
        float totalW = static_cast<float>(mapGrid_.width) * mapGrid_.tileSize;
        float totalH = static_cast<float>(mapGrid_.height) * mapGrid_.tileSize;
        char stageInfoBuf[128];
        snprintf(stageInfoBuf, sizeof(stageInfoBuf), "ステージ範囲: 幅%.0fm (%dマス) x 奥行%.0fm (%dマス) | 手前(緑):START -> 奥(黄):GOAL",
                 totalW, mapGrid_.width, totalH, mapGrid_.height);

        ImVec2 banner2Min = ImVec2(viewportScreenPos_.x + 10.0f, viewportScreenPos_.y + 38.0f);
        ImVec2 banner2Max = ImVec2(viewportScreenPos_.x + 560.0f, viewportScreenPos_.y + 60.0f);
        drawList->AddRectFilled(banner2Min, banner2Max, kBannerBg, 4.0f);
        drawList->AddRect(banner2Min, banner2Max, kBannerBorder, 4.0f);
        drawList->AddText(ImVec2(banner2Min.x + 8.0f, banner2Min.y + 3.0f), kSubTextColor, stageInfoBuf);

        // (C) ホバー中の3Dセル座標表示 (右上)
        if (isHoveredValid_) {
            float startX = -totalW * kHalfFactor;
            float startZ = -totalH * kHalfFactor;
            float worldX = startX + (static_cast<float>(hoveredGridX_) + kHalfFactor) * mapGrid_.tileSize;
            float worldZ = startZ + (static_cast<float>(hoveredGridZ_) + kHalfFactor) * mapGrid_.tileSize;

            char coordBuf[96];
            snprintf(coordBuf, sizeof(coordBuf), "3Dグリッド: [%d, %d] (X: %.1fm, Z: %.1fm)",
                     hoveredGridX_, hoveredGridZ_, worldX, worldZ);
            ImVec2 coordSize = ImGui::CalcTextSize(coordBuf);
            ImVec2 cMin = ImVec2(viewportScreenPos_.x + vpWidth - coordSize.x - 24.0f, viewportScreenPos_.y + 10.0f);
            ImVec2 cMax = ImVec2(viewportScreenPos_.x + vpWidth - 10.0f, viewportScreenPos_.y + 34.0f);
            drawList->AddRectFilled(cMin, cMax, kBannerBg, 4.0f);
            drawList->AddRect(cMin, cMax, kHighlightColor, 4.0f);
            drawList->AddText(ImVec2(cMin.x + 8.0f, cMin.y + 4.0f), kHighlightColor, coordBuf);
        }
    }
}

void MapStudioWindow::DrawControlBar() {
    ImGui::Spacing();

    // ツール切り替えボタン
    const bool isBuilding = (activeTool_ == kTileBuilding);
    if (isBuilding) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.3f, 1.0f));
    }
    if (ImGui::Button("■ ビル配置", ImVec2(100, 26))) {
        activeTool_ = kTileBuilding;
    }
    if (isBuilding) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    const bool isEmpty = (activeTool_ == kTileEmpty);
    if (isEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
    }
    if (ImGui::Button("□ 空白 (消去)", ImVec2(110, 26))) {
        activeTool_ = kTileEmpty;
    }
    if (isEmpty) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // 背景色選択
    ImGui::SetNextItemWidth(130.0f);
    ImGui::Combo("##BgColor", &selectedBgColorIndex_, kBgColorNames, IM_ARRAYSIZE(kBgColorNames));

    ImGui::SameLine();
    if (ImGui::Button("見下ろし視点")) {
        SetTopDownView();
    }
    ImGui::SameLine();
    if (ImGui::Button("カメラリセット")) {
        ResetCamera();
    }
}

void MapStudioWindow::DrawStudioInspector() {
    constexpr ImVec4 kHeaderColor = { 0.4f, 0.75f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "スタジオ・インスペクター");
    ImGui::Separator();

    // ▼ 1. マップ基本設定
    if (ImGui::CollapsingHeader("1. マップ基本設定 (Map Grid)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);
        int w = mapGrid_.width;
        int h = mapGrid_.height;
        float ts = mapGrid_.tileSize;

        if (ImGui::SliderInt("幅 (Xマス)##GridW", &w, 10, 60)) {
            mapGrid_.Reset(w, h);
        }
        if (ImGui::SliderInt("奥行き (Zマス)##GridH", &h, 10, 100)) {
            mapGrid_.Reset(w, h);
        }
        if (ImGui::SliderFloat("マスサイズ (m)##GridTileSize", &ts, 2.0f, 10.0f, "%.1f m")) {
            mapGrid_.tileSize = ts;
        }

        ImGui::TextDisabled("総ワールドサイズ: %.0f m x %.0f m",
                            static_cast<float>(mapGrid_.width) * mapGrid_.tileSize,
                            static_cast<float>(mapGrid_.height) * mapGrid_.tileSize);
        ImGui::Unindent(8.0f);
    }

    // ▼ 2. ビル設定
    if (ImGui::CollapsingHeader("2. ビル設定 (Building Settings)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);
        ImGui::SliderFloat("ビルの高さ (m)##BldgH", &mapGrid_.buildingHeight, 4.0f, 40.0f, "%.1f m");
        float col[4] = { mapGrid_.buildingColor.x, mapGrid_.buildingColor.y, mapGrid_.buildingColor.z, mapGrid_.buildingColor.w };
        if (ImGui::ColorEdit4("ビルの色##BldgCol", col)) {
            mapGrid_.buildingColor = { col[0], col[1], col[2], col[3] };
        }
        ImGui::Unindent(8.0f);
    }

    // ▼ 3. エリア選択プリセット
    if (ImGui::CollapsingHeader("3. ステージ・エリア切替 (Area)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);
        ImGui::TextUnformatted("ステージエリアを選択して読込:");
        if (ImGui::Button("Area 0 (Z: 0~120m)")) {
            currentMapName_ = "Stage1_Area0";
            strcpy_s(mapNameBuffer_, currentMapName_.c_str());
            OpenMapAsset("resources/Maps/" + currentMapName_ + ".json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Area 1 (Z: 120~240m)")) {
            currentMapName_ = "Stage1_Area1";
            strcpy_s(mapNameBuffer_, currentMapName_.c_str());
            OpenMapAsset("resources/Maps/" + currentMapName_ + ".json");
        }
        if (ImGui::Button("Area 2 (Z: 240~360m)")) {
            currentMapName_ = "Stage1_Area2";
            strcpy_s(mapNameBuffer_, currentMapName_.c_str());
            OpenMapAsset("resources/Maps/" + currentMapName_ + ".json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Area 3 (Z: 360~480m)")) {
            currentMapName_ = "Stage1_Area3";
            strcpy_s(mapNameBuffer_, currentMapName_.c_str());
            OpenMapAsset("resources/Maps/" + currentMapName_ + ".json");
        }
        ImGui::Unindent(8.0f);
    }

    // ▼ 4. アセット管理
    if (ImGui::CollapsingHeader("4. アセット管理 (File & Reset)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);
        if (ImGui::Button("ファイル選択して保存...")) {
            OpenSaveFileDialog();
        }
        ImGui::SameLine();
        if (ImGui::Button("ファイル選択して読込...")) {
            OpenLoadFileDialog();
        }

        ImGui::Spacing();
        if (ImGui::Button("全消去 (全ビル削除)...")) {
            showResetModal_ = true;
        }
        ImGui::Unindent(8.0f);
    }

    // 全消去確認モーダル
    if (showResetModal_) {
        ImGui::OpenPopup("全消去確認##StudioResetConfirm");
    }
    if (ImGui::BeginPopupModal("全消去確認##StudioResetConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "【警告】配置されているすべてのビルを消去します。");
        ImGui::Text("本当によろしいですか？");
        ImGui::Separator();
        if (ImGui::Button("はい、全消去します", ImVec2(140, 0))) {
            MapGrid before = mapGrid_;
            mapGrid_.Reset(mapGrid_.width, mapGrid_.height);
            UndoStep step;
            step.isSnapshot = true;
            step.snapshotBefore = std::move(before);
            step.snapshotAfter = mapGrid_;
            undoStack_.push_back(std::move(step));
            redoStack_.clear();
            showResetModal_ = false;
            statusMessage_ = "マップを全消去しました";
            isStatusSuccess_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(90, 0))) {
            showResetModal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ステータスメッセージ
    if (!statusMessage_.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImVec4 col = isStatusSuccess_ ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(col, "%s", statusMessage_.c_str());
    }
}

void MapStudioWindow::PlaceBuilding(int x, int z) {
    if (x < 0 || x >= mapGrid_.width || z < 0 || z >= mapGrid_.height) return;
    int prev = mapGrid_.GetTile(x, z);
    if (prev == kTileBuilding) return;

    mapGrid_.SetTile(x, z, kTileBuilding);
    currentBatch_.push_back({ x, z, prev, kTileBuilding });
}

void MapStudioWindow::RemoveBuilding(int x, int z) {
    if (x < 0 || x >= mapGrid_.width || z < 0 || z >= mapGrid_.height) return;
    int prev = mapGrid_.GetTile(x, z);
    if (prev == kTileEmpty) return;

    mapGrid_.SetTile(x, z, kTileEmpty);
    currentBatch_.push_back({ x, z, prev, kTileEmpty });
}

void MapStudioWindow::ExecuteUndo() {
    if (undoStack_.empty()) return;
    UndoStep step = std::move(undoStack_.back());
    undoStack_.pop_back();

    if (step.isSnapshot) {
        mapGrid_ = step.snapshotBefore;
    } else {
        for (auto it = step.actions.rbegin(); it != step.actions.rend(); ++it) {
            mapGrid_.SetTile(it->x, it->z, it->prevTile);
        }
    }

    redoStack_.push_back(std::move(step));
    statusMessage_ = "元に戻しました (Undo)";
    isStatusSuccess_ = true;
}

void MapStudioWindow::ExecuteRedo() {
    if (redoStack_.empty()) return;
    UndoStep step = std::move(redoStack_.back());
    redoStack_.pop_back();

    if (step.isSnapshot) {
        mapGrid_ = step.snapshotAfter;
    } else {
        for (const auto& act : step.actions) {
            mapGrid_.SetTile(act.x, act.z, act.nextTile);
        }
    }

    undoStack_.push_back(std::move(step));
    statusMessage_ = "やり直しました (Redo)";
    isStatusSuccess_ = true;
}

bool MapStudioWindow::SaveMapAsset(const std::string& filePath) {
    std::string path = filePath.empty() ? ("resources/Maps/" + currentMapName_ + ".json") : filePath;
    const std::string ext = ".json";
    if (path.size() < ext.size() || path.substr(path.size() - ext.size()) != ext) {
        path += ext;
    }

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        statusMessage_ = "保存失敗: " + path;
        isStatusSuccess_ = false;
        return false;
    }

    auto bldgs = mapGrid_.GetBuildingCoords();
    ofs << "{\n";
    ofs << "  \"map\": {\n";
    ofs << "    \"width\": " << mapGrid_.width << ",\n";
    ofs << "    \"height\": " << mapGrid_.height << ",\n";
    ofs << "    \"tileSize\": " << mapGrid_.tileSize << ",\n";
    ofs << "    \"buildingHeight\": " << mapGrid_.buildingHeight << "\n";
    ofs << "  },\n";
    ofs << "  \"buildings\": [\n";

    for (size_t i = 0; i < bldgs.size(); ++i) {
        if (i > 0) ofs << ",\n";
        ofs << "    { \"x\": " << bldgs[i].x << ", \"z\": " << bldgs[i].z << " }";
    }

    ofs << "\n  ]\n";
    ofs << "}\n";
    ofs.close();

    statusMessage_ = "保存成功: " + path + " (" + std::to_string(bldgs.size()) + " 棟)";
    isStatusSuccess_ = true;
    return true;
}

bool MapStudioWindow::OpenMapAsset(const std::string& filePath) {
    std::string path = filePath.empty() ? ("resources/Maps/" + currentMapName_ + ".json") : filePath;
    const std::string ext = ".json";
    if (path.size() < ext.size() || path.substr(path.size() - ext.size()) != ext) {
        path += ext;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        statusMessage_ = "読込失敗: ファイルがありません (" + path + ")";
        isStatusSuccess_ = false;
        return false;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string jsonStr = ss.str();
    ifs.close();

    MapGrid before = mapGrid_;

    auto extractVal = [](const std::string& block, const std::string& key, int def) -> int {
        size_t p = block.find("\"" + key + "\":");
        if (p == std::string::npos) return def;
        size_t vs = p + key.length() + 3;
        while (vs < block.length() && (block[vs] == ' ' || block[vs] == '\t')) ++vs;
        try { return std::stoi(block.substr(vs)); } catch (...) { return def; }
    };

    auto extractFloatVal = [](const std::string& block, const std::string& key, float def) -> float {
        size_t p = block.find("\"" + key + "\":");
        if (p == std::string::npos) return def;
        size_t vs = p + key.length() + 3;
        while (vs < block.length() && (block[vs] == ' ' || block[vs] == '\t')) ++vs;
        try { return std::stof(block.substr(vs)); } catch (...) { return def; }
    };

    int w = kDefaultGridWidth;
    int h = kDefaultGridHeight;
    float ts = kDefaultTileSize;
    float bh = kDefaultBuildingHeight;

    size_t mapPos = jsonStr.find("\"map\"");
    if (mapPos != std::string::npos) {
        size_t mapStart = jsonStr.find('{', mapPos);
        size_t mapEnd = jsonStr.find('}', mapStart);
        if (mapStart != std::string::npos && mapEnd != std::string::npos) {
            std::string mb = jsonStr.substr(mapStart, mapEnd - mapStart + 1);
            w = extractVal(mb, "width", kDefaultGridWidth);
            h = extractVal(mb, "height", kDefaultGridHeight);
            ts = extractFloatVal(mb, "tileSize", kDefaultTileSize);
            bh = extractFloatVal(mb, "buildingHeight", kDefaultBuildingHeight);
        }
    }

    mapGrid_.tileSize = ts;
    mapGrid_.buildingHeight = bh;
    mapGrid_.Reset(w, h);

    int loadedCount = 0;
    // 1. "buildings" 配列の読み込み
    size_t bPos = jsonStr.find("\"buildings\"");
    if (bPos != std::string::npos) {
        size_t arrStart = jsonStr.find('[', bPos);
        size_t arrEnd = jsonStr.rfind(']');
        if (arrStart != std::string::npos && arrEnd != std::string::npos && arrEnd > arrStart) {
            size_t cur = arrStart + 1;
            while (cur < arrEnd) {
                size_t oStart = jsonStr.find('{', cur);
                if (oStart == std::string::npos || oStart >= arrEnd) break;
                size_t oEnd = jsonStr.find('}', oStart);
                if (oEnd == std::string::npos || oEnd > arrEnd) break;

                std::string block = jsonStr.substr(oStart, oEnd - oStart + 1);
                int gx = extractVal(block, "x", -1);
                int gz = extractVal(block, "z", -1);

                if (gx >= 0 && gx < w && gz >= 0 && gz < h) {
                    mapGrid_.SetTile(gx, gz, kTileBuilding);
                    loadedCount++;
                }
                cur = oEnd + 1;
            }
        }
    } else {
        // 互換性: "tiles" 配列形式の読み込み（id == 1 または id == 2 のビル）
        size_t tPos = jsonStr.find("\"tiles\"");
        if (tPos != std::string::npos) {
            size_t arrStart = jsonStr.find('[', tPos);
            size_t arrEnd = jsonStr.rfind(']');
            if (arrStart != std::string::npos && arrEnd != std::string::npos && arrEnd > arrStart) {
                size_t cur = arrStart + 1;
                while (cur < arrEnd) {
                    size_t oStart = jsonStr.find('{', cur);
                    if (oStart == std::string::npos || oStart >= arrEnd) break;
                    size_t oEnd = jsonStr.find('}', oStart);
                    if (oEnd == std::string::npos || oEnd > arrEnd) break;

                    std::string block = jsonStr.substr(oStart, oEnd - oStart + 1);
                    int gx = extractVal(block, "x", -1);
                    int gz = extractVal(block, "z", -1);
                    int id = extractVal(block, "id", 0);

                    if (gx >= 0 && gx < w && gz >= 0 && gz < h && id > 0) {
                        mapGrid_.SetTile(gx, gz, kTileBuilding);
                        loadedCount++;
                    }
                    cur = oEnd + 1;
                }
            }
        }
    }

    UndoStep step;
    step.isSnapshot = true;
    step.snapshotBefore = std::move(before);
    step.snapshotAfter = mapGrid_;
    undoStack_.push_back(std::move(step));
    redoStack_.clear();

    statusMessage_ = "読込成功: " + path + " (" + std::to_string(loadedCount) + " 棟)";
    isStatusSuccess_ = true;
    return true;
}

void MapStudioWindow::OpenSaveFileDialog() {
    wchar_t buf[kMaxPathLength] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(kMaxPathLength);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    if (GetSaveFileNameW(&ofn) == TRUE) {
        SaveMapAsset(WideToUTF8(buf));
    }
}

void MapStudioWindow::OpenLoadFileDialog() {
    wchar_t buf[kMaxPathLength] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(kMaxPathLength);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    if (GetOpenFileNameW(&ofn) == TRUE) {
        OpenMapAsset(WideToUTF8(buf));
    }
}

#endif // _USEIMGUI
