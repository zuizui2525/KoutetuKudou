#ifdef _USEIMGUI
#include "Engine/Debug/MapStudioWindow.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Math/Matrix/Matrix.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
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
    // 背景色プリセット定数 (マジックナンバー排除)
    constexpr Vector4 kBgColorDarkGray  = { 0.12f, 0.12f, 0.14f, 1.0f };
    constexpr Vector4 kBgColorPureBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr Vector4 kBgColorBlue      = { 0.08f, 0.14f, 0.25f, 1.0f };

    constexpr const char* kBgColorNames[] = {
        "ダークグレー (標準)",
        "漆黒 (ブラック)",
        "ディープブルー"
    };

    // カメラ定数 (Near/Far比を適正化し24bit深度バッファの精度を劇的に向上)
    constexpr float kFovY = 45.0f * (3.14159265f / 180.0f);
    constexpr float kNearZ = 1.0f;
    constexpr float kFarZ = 600.0f;
    constexpr float kHalfFactor = 0.5f;
    constexpr float kControlBarHeight = 44.0f;
    constexpr float kPreviewHoverScaleBoost = 1.03f;
    constexpr float kRaycastEpsilon = 1e-5f;

    // 地面プレート・外枠・土台定数 (マジックナンバー排除)
    constexpr float kGroundBaseMargin = 8.0f;        // ステージ外側に広げる暗色ベース土台のマージン (m)
    constexpr float kGroundBaseThickness = 0.4f;     // ベース土台の厚み (m)
    constexpr float kGroundBaseYOffset = -0.3f;      // ベース土台の中心Y (上面 Y = -0.1f)
    constexpr float kStageGroundThickness = 0.2f;    // ステージ作業エリア床面の厚み (m)
    constexpr float kStageGroundYOffset = -0.115f;   // ステージ作業エリア床面の中心Y (上面 Y = -0.015m: グリッド線とのZ干渉を完全根絶)
    constexpr float kCornerPoleSize = 0.35f;         // 四隅のコーナーポールの太さ (m)
    constexpr float kCornerPoleHeight = 1.60f;       // 四隅のコーナーポールの高さ (m)

    // グリッド線・境界線定数 (3D直方体Cubeメッシュによる確実な立体描画 / マジックナンバー排除)
    constexpr float kGridLineBoxHeight = 0.060f;         // 通常グリッド線の厚み (m: 床面から十分に浮揚)
    constexpr float kGridLineBoxY = 0.030f;              // 通常グリッド線の中心Y (上面 Y = 0.060m)
    constexpr float kMajorGridLineBoxHeight = 0.080f;    // 5マス主グリッド線の厚み (m)
    constexpr float kMajorGridLineBoxY = 0.040f;         // 5マス主グリッド線の中心Y (上面 Y = 0.080m)
    constexpr float kBorderLineBoxHeight = 0.100f;       // ステージ外枠境界線の厚み (m)
    constexpr float kBorderLineBoxY = 0.050f;           // ステージ外枠境界線の中心Y (上面 Y = 0.100m)
    constexpr float kHoverBorderBoxHeight = 0.120f;      // ホバー中マス枠線の厚み (m)
    constexpr float kHoverBorderBoxY = 0.060f;           // ホバー中マス枠線の中心Y (上面 Y = 0.120m)

    constexpr float kGridLineThickness = 0.14f;          // 通常グリッド線幅 (m: 遠景でも途切れない14cm幅)
    constexpr float kMajorGridLineThickness = 0.22f;     // 5マスごとの主グリッド線幅 (m: くっきり見える22cm幅)
    constexpr float kBorderLineThickness = 0.28f;        // ステージ外枠境界線幅 (m)
    constexpr float kStartGoalLineThickness = 0.36f;     // START/GOALライン幅 (m)
    constexpr float kHoverBorderThickness = 0.18f;       // 接地ホバー枠幅 (m)
    constexpr int   kMajorGridInterval = 5;              // 主線の間隔 (5マスごと)

    // カラーパレット定数 (ハイコントラスト・高視認性)
    constexpr Vector4 kGroundBaseColor         = { 0.08f, 0.09f, 0.12f, 1.0f }; // 外側の下層ベース土台 (暗色)
    constexpr Vector4 kStageGroundColor        = { 0.14f, 0.16f, 0.22f, 1.0f }; // ステージ作業エリア床面 (落ち着いたダークスレートブルー)
    constexpr Vector4 kGridLineColor           = { 0.55f, 0.65f, 0.78f, 1.0f }; // 通常グリッド線 (清潔感のある高視認性白水色)
    constexpr Vector4 kGridMajorLineColor      = { 0.88f, 0.94f, 1.00f, 1.0f };  // 5マスごとの主グリッド線 (くっきり目立つハイライトホワイト)
    constexpr Vector4 kStageBorderColor        = { 0.10f, 0.85f, 1.00f, 1.0f };  // ステージ外枠境界 (高輝度シアン)
    constexpr Vector4 kStartLineColor          = { 0.15f, 1.00f, 0.45f, 1.0f };  // START手前ライン (高輝度ライムグリーン)
    constexpr Vector4 kGoalLineColor           = { 1.00f, 0.85f, 0.15f, 1.0f };  // GOAL奥ライン (高輝度ゴールド)
    constexpr Vector4 kCornerPoleColor         = { 0.10f, 0.85f, 1.00f, 1.0f };  // 四隅コーナーポスト (高輝度シアン)
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

    // XZ平面上で帯状の線分 (Quad: 2三角形 6頂点) を生成するヘルパー
    void AddQuadLine(std::vector<VertexData>& vertices, const Vector3& start, const Vector3& end, float thickness, float y) {
        float dx = end.x - start.x;
        float dz = end.z - start.z;
        float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-5f) return;

        // XZ平面上で進行方向に直交する法線 (太さ方向)
        float nx = -dz / len;
        float nz = dx / len;
        float halfTh = thickness * 0.5f;

        float ox = nx * halfTh;
        float oz = nz * halfTh;

        VertexData v0{}, v1{}, v2{}, v3{};
        v0.position = { start.x - ox, y, start.z - oz, 1.0f };
        v0.normal = { 0.0f, 1.0f, 0.0f };
        v0.texcoord = { 0.0f, 0.0f };

        v1.position = { end.x - ox, y, end.z - oz, 1.0f };
        v1.normal = { 0.0f, 1.0f, 0.0f };
        v1.texcoord = { 0.0f, 1.0f };

        v2.position = { start.x + ox, y, start.z + oz, 1.0f };
        v2.normal = { 0.0f, 1.0f, 0.0f };
        v2.texcoord = { 1.0f, 0.0f };

        v3.position = { end.x + ox, y, end.z + oz, 1.0f };
        v3.normal = { 0.0f, 1.0f, 0.0f };
        v3.texcoord = { 1.0f, 1.0f };

        // 三角形1
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);

        // 三角形2
        vertices.push_back(v2);
        vertices.push_back(v1);
        vertices.push_back(v3);
    }

    // 3D直方体 (Box: 6面 12三角形 36頂点 / 時計回りCW頂点順で全外面を確実に描画) を生成するヘルパー
    void AddQuadBox(std::vector<VertexData>& vertices, const Vector3& center, const Vector3& size) {
        float hx = size.x * 0.5f;
        float hy = size.y * 0.5f;
        float hz = size.z * 0.5f;

        auto addFace = [&](const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& norm) {
            VertexData v0{ { p0.x, p0.y, p0.z, 1.0f }, { 0.0f, 0.0f }, norm };
            VertexData v1{ { p1.x, p1.y, p1.z, 1.0f }, { 0.0f, 1.0f }, norm };
            VertexData v2{ { p2.x, p2.y, p2.z, 1.0f }, { 1.0f, 0.0f }, norm };
            VertexData v3{ { p3.x, p3.y, p3.z, 1.0f }, { 1.0f, 1.0f }, norm };
            // CW: (左下->左上->右下), (右下->左上->右上)
            vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
            vertices.push_back(v2); vertices.push_back(v1); vertices.push_back(v3);
        };

        // 前面 (-Z: 手前側)
        addFace({ center.x - hx, center.y - hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y - hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z - hz }, { 0.0f, 0.0f, -1.0f });
        // 背面 (+Z: 奥側)
        addFace({ center.x + hx, center.y - hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z + hz },
                { center.x - hx, center.y - hy, center.z + hz },
                { center.x - hx, center.y + hy, center.z + hz }, { 0.0f, 0.0f, 1.0f });
        // 左面 (-X: 左側面) - 奥(+Z)から手前(-Z)へ時計回り (CubeObject準拠)
        addFace({ center.x - hx, center.y - hy, center.z + hz },
                { center.x - hx, center.y + hy, center.z + hz },
                { center.x - hx, center.y - hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z - hz }, { -1.0f, 0.0f, 0.0f });
        // 右面 (+X: 右側面) - 手前(-Z)から奥(+Z)へ時計回り (CubeObject準拠)
        addFace({ center.x + hx, center.y - hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y - hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z + hz }, { 1.0f, 0.0f, 0.0f });
        // 上面 (+Y: 屋根)
        addFace({ center.x - hx, center.y + hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z + hz }, { 0.0f, 1.0f, 0.0f });
        // 底面 (-Y: 接地面)
        addFace({ center.x - hx, center.y - hy, center.z + hz },
                { center.x - hx, center.y - hy, center.z - hz },
                { center.x + hx, center.y - hy, center.z + hz },
                { center.x + hx, center.y - hy, center.z - hz }, { 0.0f, -1.0f, 0.0f });
    }

    // ビル専用直方体 (底面 -Y を除外した5面10三角形 / 時計回りCW頂点順で前面手前も完全に四角形として描画)
    void AddBuildingBox(std::vector<VertexData>& vertices, const Vector3& center, const Vector3& size) {
        float hx = size.x * 0.5f;
        float hy = size.y * 0.5f;
        float hz = size.z * 0.5f;

        auto addFace = [&](const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& norm) {
            VertexData v0{ { p0.x, p0.y, p0.z, 1.0f }, { 0.0f, 0.0f }, norm };
            VertexData v1{ { p1.x, p1.y, p1.z, 1.0f }, { 0.0f, 1.0f }, norm };
            VertexData v2{ { p2.x, p2.y, p2.z, 1.0f }, { 1.0f, 0.0f }, norm };
            VertexData v3{ { p3.x, p3.y, p3.z, 1.0f }, { 1.0f, 1.0f }, norm };
            // CW: (左下->左上->右下), (右下->左上->右上)
            vertices.push_back(v0); vertices.push_back(v1); vertices.push_back(v2);
            vertices.push_back(v2); vertices.push_back(v1); vertices.push_back(v3);
        };

        // 前面 (-Z: 手前側)
        addFace({ center.x - hx, center.y - hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y - hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z - hz }, { 0.0f, 0.0f, -1.0f });
        // 背面 (+Z: 奥側)
        addFace({ center.x + hx, center.y - hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z + hz },
                { center.x - hx, center.y - hy, center.z + hz },
                { center.x - hx, center.y + hy, center.z + hz }, { 0.0f, 0.0f, 1.0f });
        // 左面 (-X: 左側面) - 奥(+Z)から手前(-Z)へ時計回り (CubeObject準拠)
        addFace({ center.x - hx, center.y - hy, center.z + hz },
                { center.x - hx, center.y + hy, center.z + hz },
                { center.x - hx, center.y - hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z - hz }, { -1.0f, 0.0f, 0.0f });
        // 右面 (+X: 右側面) - 手前(-Z)から奥(+Z)へ時計回り (CubeObject準拠)
        addFace({ center.x + hx, center.y - hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y - hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z + hz }, { 1.0f, 0.0f, 0.0f });
        // 上面 (+Y: 屋根)
        addFace({ center.x - hx, center.y + hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z + hz }, { 0.0f, 1.0f, 0.0f });
        // ※底面 (-Y) は意図的に除外して床面との重複Z-fightingを防止
    }

    // 頂点バッファをGPUリソースへアップロードするヘルパー (遅延解放キューで安全に旧リソースを破棄)
    void UploadVertexBuffer(ID3D12Device* device, const std::vector<VertexData>& vertices,
                            Microsoft::WRL::ComPtr<ID3D12Resource>& outResource,
                            D3D12_VERTEX_BUFFER_VIEW& outVbView, uint32_t& outCount) {
        auto deferredMgr = DeferredReleaseManager::GetInstance();
        if (outResource && deferredMgr) {
            deferredMgr->Enqueue(std::move(outResource));
        }
        outCount = static_cast<uint32_t>(vertices.size());
        if (vertices.empty() || !device) {
            outVbView = {};
            return;
        }
        size_t sizeInBytes = sizeof(VertexData) * vertices.size();
        outResource = DxUtils::CreateBufferResource(device, sizeInBytes);
        if (!outResource) return;

        void* mapped = nullptr;
        HRESULT hr = outResource->Map(0, nullptr, &mapped);
        if (SUCCEEDED(hr) && mapped) {
            memcpy(mapped, vertices.data(), sizeInBytes);
            outResource->Unmap(0, nullptr);
        }
        outVbView.BufferLocation = outResource->GetGPUVirtualAddress();
        outVbView.SizeInBytes = static_cast<UINT>(sizeInBytes);
        outVbView.StrideInBytes = sizeof(VertexData);
    }
}

MapStudioWindow::MapStudioWindow() {
    studioCamera_ = std::make_shared<BaseCamera>();
    studioCamera_->Initialize();
    ResetCamera();
}

MapStudioWindow::~MapStudioWindow() {
    // 1. CameraManager からのカメラ登録解除 (ダングリングポインタ・アクセス違反を完全防止)
    if (auto cameraMgr = CameraResource::GetCameraManager()) {
        cameraMgr->RemoveCamera(CameraManager::kMapStudioCameraName);
    }

    // 2. プレビュー用レンダーテクスチャおよび深度ステンシルの明示的解放
    previewRenderTexture_.reset();
    depthStencilResource_.Reset();
    dsvHeap_.Reset();
    rtvHeap_.Reset();

    // 3. バッチ用頂点バッファの解放
    gridVbResource_.Reset();
    majorGridVbResource_.Reset();
    borderVbResource_.Reset();
    startLineVbResource_.Reset();
    goalLineVbResource_.Reset();
    hoverBorderVbResource_.Reset();
    buildingVbResource_.Reset();

    // 4. 独立 3D オブジェクトの明示的解放
    previewGroundBase_.reset();
    previewStageGround_.reset();
    previewCornerPole_.reset();
    previewBuildingCube_.reset();
    previewHoverCube_.reset();
    previewLine_.reset();

    gridBatchObject_.reset();
    majorGridBatchObject_.reset();
    borderBatchObject_.reset();
    startLineBatchObject_.reset();
    goalLineBatchObject_.reset();
    hoverBorderBatchObject_.reset();

    studioCamera_.reset();
}

void MapStudioWindow::ResetCamera() {
    float totalH = static_cast<float>(mapGrid_.height) * mapGrid_.tileSize;
    cameraTarget_ = { 0.0f, 0.0f, 0.0f };
    cameraDistance_ = (std::max)(50.0f, totalH * 0.85f);
    cameraPitch_ = 0.62f; // 俯瞰約35度
    cameraYaw_ = 0.0f;

    Matrix4x4 rot = Math::MakeRotateMatrix(cameraPitch_, cameraYaw_, 0.0f);
    Vector3 forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rot);
    cameraPosition_ = cameraTarget_ - forward * cameraDistance_;
    cameraRotation_ = { cameraPitch_, cameraYaw_, 0.0f };

    if (studioCamera_) {
        studioCamera_->SetPosition(cameraPosition_);
        studioCamera_->SetRotation(cameraRotation_);
        studioCamera_->Update();
    }
}

void MapStudioWindow::SetTopDownView() {
    float totalH = static_cast<float>(mapGrid_.height) * mapGrid_.tileSize;
    cameraTarget_ = { 0.0f, 0.0f, 0.0f };
    cameraDistance_ = (std::max)(60.0f, totalH * 1.05f);
    cameraPitch_ = 1.54f; // ほぼ真上見下ろし (約88度)
    cameraYaw_ = 0.0f;

    Matrix4x4 rot = Math::MakeRotateMatrix(cameraPitch_, cameraYaw_, 0.0f);
    Vector3 forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rot);
    cameraPosition_ = cameraTarget_ - forward * cameraDistance_;
    cameraRotation_ = { cameraPitch_, cameraYaw_, 0.0f };

    if (studioCamera_) {
        studioCamera_->SetPosition(cameraPosition_);
        studioCamera_->SetRotation(cameraRotation_);
        studioCamera_->Update();
    }
}

void MapStudioWindow::UpdateCamera() {
    if (!studioCamera_) return;

    ImGuiIO& io = ImGui::GetIO();
    bool isShift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    bool isCtrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);

    // 操作開始・終了判定 (ビューポート内でクリック開始されたらドラッグ枠外でもボタンを離すまで操作継続)
    if (!isFlyNavigating_ && isViewportHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        isOrbitNavigating_ = true;
    }
    if (isOrbitNavigating_ && !io.MouseDown[2]) {
        isOrbitNavigating_ = false;
    }

    if (!isOrbitNavigating_ && isViewportHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        isFlyNavigating_ = true;
    }
    if (isFlyNavigating_ && !io.MouseDown[1]) {
        isFlyNavigating_ = false;
    }

    isCameraNavigating_ = isOrbitNavigating_ || isFlyNavigating_;

    Matrix4x4 rotateMatrix = Math::MakeRotateMatrix(cameraPitch_, cameraYaw_, 0.0f);
    Vector3 forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
    Vector3 right   = Math::TransformNormal({ 1.0f, 0.0f, 0.0f }, rotateMatrix);
    Vector3 up      = Math::TransformNormal({ 0.0f, 1.0f, 0.0f }, rotateMatrix);

    // 1. Blender風 MMB (マウス中ボタン) 操作 (オービット回転 / Shiftパン / Ctrlズーム)
    if (isOrbitNavigating_) {
        if (isShift) {
            // (A) Shift + MMB: 画面XY平面に沿った平行移動 (Pan)
            float panFactor = cameraDistance_ * kPanSpeedFactor;
            Vector3 deltaMove = (-right * io.MouseDelta.x + up * io.MouseDelta.y) * panFactor;
            cameraTarget_ = cameraTarget_ + deltaMove;
        } else if (isCtrl) {
            // (B) Ctrl + MMB: ドリーズーム (Zoom)
            constexpr float kCtrlZoomFactorMultiplier = 0.05f;
            cameraDistance_ += io.MouseDelta.y * (cameraDistance_ * kZoomWheelFactor * kCtrlZoomFactorMultiplier);
            cameraDistance_ = std::clamp(cameraDistance_, kMinCameraDistance, kMaxCameraDistance);
        } else {
            // (C) 単体 MMB: 注視点中心のオービット回転 (Orbit)
            cameraYaw_ += io.MouseDelta.x * kOrbitRotateSpeed;
            cameraPitch_ += io.MouseDelta.y * kOrbitRotateSpeed;
            cameraPitch_ = std::clamp(cameraPitch_, -kMaxCameraPitch, kMaxCameraPitch);
        }

        // カメラ位置の更新
        rotateMatrix = Math::MakeRotateMatrix(cameraPitch_, cameraYaw_, 0.0f);
        forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
        cameraPosition_ = cameraTarget_ - forward * cameraDistance_;
        cameraRotation_ = { cameraPitch_, cameraYaw_, 0.0f };
    }
    // 2. 右クリック + WASD (フライスルー移動モード: 既存のゲームエンジン風操作との完全両立)
    else if (isFlyNavigating_) {
        // マウス移動による視線回転 (ピッチ・ヨー)
        cameraYaw_ += io.MouseDelta.x * kCameraRotateSpeed;
        cameraPitch_ += io.MouseDelta.y * kCameraRotateSpeed;
        cameraPitch_ = std::clamp(cameraPitch_, -kMaxCameraPitch, kMaxCameraPitch);

        rotateMatrix = Math::MakeRotateMatrix(cameraPitch_, cameraYaw_, 0.0f);
        forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
        right   = Math::TransformNormal({ 1.0f, 0.0f, 0.0f }, rotateMatrix);
        Vector3 worldUp = { 0.0f, 1.0f, 0.0f };

        auto input = InputResource::GetInput();
        Vector3 move = { 0.0f, 0.0f, 0.0f };
        if (ImGui::IsKeyDown(ImGuiKey_W) || (input && input->Press(DIK_W))) move = move + forward;
        if (ImGui::IsKeyDown(ImGuiKey_S) || (input && input->Press(DIK_S))) move = move - forward;
        if (ImGui::IsKeyDown(ImGuiKey_D) || (input && input->Press(DIK_D))) move = move + right;
        if (ImGui::IsKeyDown(ImGuiKey_A) || (input && input->Press(DIK_A))) move = move - right;
        if (ImGui::IsKeyDown(ImGuiKey_Space) || ImGui::IsKeyDown(ImGuiKey_E) || (input && (input->Press(DIK_SPACE) || input->Press(DIK_E)))) move = move + worldUp;
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_Q) || (input && (input->Press(DIK_LCONTROL) || input->Press(DIK_Q)))) move = move - worldUp;

        if (Math::Length(move) > 0.0f) {
            float speed = kCameraMoveSpeed;
            if (isShift || (input && (input->Press(DIK_LSHIFT) || input->Press(DIK_RSHIFT)))) {
                speed *= kCameraFastMultiplier;
            }
            move = Math::Normalize(move) * speed;
            cameraPosition_ = cameraPosition_ + move;
            cameraTarget_ = cameraTarget_ + move; // 注視点も同量追従
        }
        cameraRotation_ = { cameraPitch_, cameraYaw_, 0.0f };
    } else {
        // 3. マウスホイールによるドリーズーム
        if (isViewportHovered_ && std::abs(io.MouseWheel) > 0.01f) {
            cameraDistance_ -= io.MouseWheel * (cameraDistance_ * kZoomWheelFactor);
            cameraDistance_ = std::clamp(cameraDistance_, kMinCameraDistance, kMaxCameraDistance);

            rotateMatrix = Math::MakeRotateMatrix(cameraPitch_, cameraYaw_, 0.0f);
            forward = Math::TransformNormal({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
            cameraPosition_ = cameraTarget_ - forward * cameraDistance_;
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

    // 5. プレビュー用3Dオブジェクト群の初期化 (DirectX 12上書き競合防止のため独立インスタンス化)
    previewGroundBase_ = std::make_unique<CubeObject>();
    previewGroundBase_->Initialize(0); // ライティング無効: メインシーンのPointLightによる白飛びを完全防止

    previewStageGround_ = std::make_unique<CubeObject>();
    previewStageGround_->Initialize(0); // ライティング無効: メインシーンのPointLightによる白飛びを完全防止

    previewBuildingCube_ = std::make_unique<CubeObject>();
    previewBuildingCube_->Initialize(2);

    previewHoverCube_ = std::make_unique<CubeObject>();
    previewHoverCube_->Initialize(0);

    gridBatchObject_ = std::make_unique<CubeObject>();
    gridBatchObject_->Initialize(0);

    majorGridBatchObject_ = std::make_unique<CubeObject>();
    majorGridBatchObject_->Initialize(0);

    borderBatchObject_ = std::make_unique<CubeObject>();
    borderBatchObject_->Initialize(0);

    startLineBatchObject_ = std::make_unique<CubeObject>();
    startLineBatchObject_->Initialize(0);

    goalLineBatchObject_ = std::make_unique<CubeObject>();
    goalLineBatchObject_->Initialize(0);

    hoverBorderBatchObject_ = std::make_unique<CubeObject>();
    hoverBorderBatchObject_->Initialize(0);

    // 6. グリッド・境界線およびビルの初回メッシュ構築
    RebuildGridAndBorderMeshes();
    RebuildBuildingMesh();

    // 7. スタジオ専用カメラを CameraManager に登録 (クリップ面と画角を最適化)
    if (!studioCamera_) {
        studioCamera_ = std::make_shared<BaseCamera>();
        studioCamera_->Initialize();
    }
    studioCamera_->SetNearClip(kNearZ);
    studioCamera_->SetFarClip(kFarZ);
    studioCamera_->SetFov(kFovY);
    auto cameraMgr = CameraResource::GetCameraManager();
    if (cameraMgr && !cameraMgr->HasCamera(CameraManager::kMapStudioCameraName)) {
        cameraMgr->AddCamera(CameraManager::kMapStudioCameraName, studioCamera_);
    }

    ResetCamera();
    // ステージ1の初期マップを自動ロードして即座に配置
    OpenMapAsset("resources/Maps/Stage1_Area0.json");

    isInitialized_ = true;
    return true;
}

void MapStudioWindow::RebuildGridAndBorderMeshes() {
    auto engine = EngineResource::GetEngine();
    if (!engine) return;
    ID3D12Device* device = engine->GetDevice();
    if (!device) return;

    const float tileSize = mapGrid_.tileSize;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float startX = -totalWidth * kHalfFactor;
    const float startZ = -totalHeight * kHalfFactor;

    std::vector<VertexData> normalGridVertices;
    std::vector<VertexData> majorGridVertices;
    std::vector<VertexData> borderVertices;
    std::vector<VertexData> startLineVertices;
    std::vector<VertexData> goalLineVertices;

    // 1. 横線 (Z方向に並ぶ線, X軸に平行な極薄直方体Cube)
    for (int z = 0; z <= mapGrid_.height; ++z) {
        float pz = startZ + static_cast<float>(z) * tileSize;
        // 外枠と重なる手前(0)と奥(height)は境界ライン側で描画するためスキップ
        if (z == 0 || z == mapGrid_.height) continue;

        bool isMajor = (z % kMajorGridInterval == 0);
        float th = isMajor ? kMajorGridLineThickness : kGridLineThickness;
        float boxH = isMajor ? kMajorGridLineBoxHeight : kGridLineBoxHeight;
        float boxY = isMajor ? kMajorGridLineBoxY : kGridLineBoxY;
        auto& targetVec = isMajor ? majorGridVertices : normalGridVertices;

        AddQuadBox(targetVec, { 0.0f, boxY, pz }, { totalWidth, boxH, th });
    }

    // 2. 縦線 (X方向に並ぶ線, Z軸に平行な極薄直方体Cube)
    for (int x = 0; x <= mapGrid_.width; ++x) {
        float px = startX + static_cast<float>(x) * tileSize;
        // 外枠と重なる左(0)と右(width)は境界ライン側で描画するためスキップ
        if (x == 0 || x == mapGrid_.width) continue;

        bool isMajor = (x % kMajorGridInterval == 0);
        float th = isMajor ? kMajorGridLineThickness : kGridLineThickness;
        float boxH = isMajor ? kMajorGridLineBoxHeight : kGridLineBoxHeight;
        float boxY = isMajor ? kMajorGridLineBoxY : kGridLineBoxY;
        auto& targetVec = isMajor ? majorGridVertices : normalGridVertices;

        AddQuadBox(targetVec, { px, boxY, 0.0f }, { th, boxH, totalHeight });
    }

    // 3. 左右境界線 (高輝度シアン直方体Cube)
    AddQuadBox(borderVertices, { startX, kBorderLineBoxY, 0.0f }, { kBorderLineThickness, kBorderLineBoxHeight, totalHeight });
    AddQuadBox(borderVertices, { startX + totalWidth, kBorderLineBoxY, 0.0f }, { kBorderLineThickness, kBorderLineBoxHeight, totalHeight });

    // 4. STARTライン (手前側境界: 高輝度ライムグリーン直方体Cube)
    AddQuadBox(startLineVertices, { 0.0f, kBorderLineBoxY, startZ }, { totalWidth, kBorderLineBoxHeight, kStartGoalLineThickness });

    // 5. GOALライン (奥側境界: 高輝度ゴールド直方体Cube)
    AddQuadBox(goalLineVertices, { 0.0f, kBorderLineBoxY, startZ + totalHeight }, { totalWidth, kBorderLineBoxHeight, kStartGoalLineThickness });

    // 6. 四隅の立体コーナーポスト (高輝度シアン直方体: どこまでが枠かを3D空間内で直感的に強調)
    const float poleY = kCornerPoleHeight * kHalfFactor;
    Vector3 poleSize = { kCornerPoleSize, kCornerPoleHeight, kCornerPoleSize };
    AddQuadBox(borderVertices, { startX, poleY, startZ }, poleSize);
    AddQuadBox(borderVertices, { startX + totalWidth, poleY, startZ }, poleSize);
    AddQuadBox(borderVertices, { startX, poleY, startZ + totalHeight }, poleSize);
    AddQuadBox(borderVertices, { startX + totalWidth, poleY, startZ + totalHeight }, poleSize);

    // GPUバッファへアップロード
    UploadVertexBuffer(device, normalGridVertices, gridVbResource_, gridVbView_, gridVertexCount_);
    UploadVertexBuffer(device, majorGridVertices, majorGridVbResource_, majorGridVbView_, majorGridVertexCount_);
    UploadVertexBuffer(device, borderVertices, borderVbResource_, borderVbView_, borderVertexCount_);
    UploadVertexBuffer(device, startLineVertices, startLineVbResource_, startLineVbView_, startLineVertexCount_);
    UploadVertexBuffer(device, goalLineVertices, goalLineVbResource_, goalLineVbView_, goalLineVertexCount_);

    cachedGridWidth_ = mapGrid_.width;
    cachedGridHeight_ = mapGrid_.height;
    cachedTileSize_ = mapGrid_.tileSize;
}

void MapStudioWindow::RebuildBuildingMesh() {
    auto engine = EngineResource::GetEngine();
    if (!engine) return;
    ID3D12Device* device = engine->GetDevice();
    if (!device) return;

    const float tileSize = mapGrid_.tileSize;
    const float buildingH = mapGrid_.buildingHeight;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float startX = -totalWidth * kHalfFactor;
    const float startZ = -totalHeight * kHalfFactor;

    std::vector<VertexData> buildingVertices;

    for (int z = 0; z < mapGrid_.height; ++z) {
        for (int x = 0; x < mapGrid_.width; ++x) {
            if (mapGrid_.GetTile(x, z) != kTileBuilding) continue;

            float posX = startX + (static_cast<float>(x) + kHalfFactor) * tileSize;
            float posZ = startZ + (static_cast<float>(z) + kHalfFactor) * tileSize;
            float posY = buildingH * kHalfFactor;

            // ★底面 (-Y) を除外した5面ボックスを生成し、床面プレート (Y=0.0f) との Z-fighting を完全に根絶
            AddBuildingBox(buildingVertices, { posX, posY, posZ }, { tileSize, buildingH, tileSize });
        }
    }

    UploadVertexBuffer(device, buildingVertices, buildingVbResource_, buildingVbView_, buildingVertexCount_);
    isBuildingMeshDirty_ = false;
}

void MapStudioWindow::RenderGridAndBorders(ID3D12GraphicsCommandList* cmdList) {
    if (!cmdList) return;

    // マップサイズ変更検知時の動的再構築
    if (cachedGridWidth_ != mapGrid_.width || cachedGridHeight_ != mapGrid_.height || cachedTileSize_ != mapGrid_.tileSize) {
        RebuildGridAndBorderMeshes();
        RebuildBuildingMesh();
    }

    auto drawer = Object3DDrawer::GetInstance();
    if (!drawer) return;

    // 1. 通常グリッド線 (薄水色グレー直方体Cube)
    if (gridBatchObject_ && gridVertexCount_ > 0) {
        gridBatchObject_->GetMaterialData()->color = kGridLineColor;
        gridBatchObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
        gridBatchObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        gridBatchObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        gridBatchObject_->Update();
        drawer->Draw(gridBatchObject_.get(), gridVbView_, gridVertexCount_, "white", "", "Object3D");
    }

    // 2. 5マスごとの主グリッド線 (明るい水色直方体Cube)
    if (majorGridBatchObject_ && majorGridVertexCount_ > 0) {
        majorGridBatchObject_->GetMaterialData()->color = kGridMajorLineColor;
        majorGridBatchObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
        majorGridBatchObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        majorGridBatchObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        majorGridBatchObject_->Update();
        drawer->Draw(majorGridBatchObject_.get(), majorGridVbView_, majorGridVertexCount_, "white", "", "Object3D");
    }

    // 3. ステージ外枠境界線および四隅コーナーポスト (高輝度シアン直方体Cube)
    if (borderBatchObject_ && borderVertexCount_ > 0) {
        borderBatchObject_->GetMaterialData()->color = kStageBorderColor;
        borderBatchObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
        borderBatchObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        borderBatchObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        borderBatchObject_->Update();
        drawer->Draw(borderBatchObject_.get(), borderVbView_, borderVertexCount_, "white", "", "Object3D");
    }

    // 4. START手前ライン (高輝度ライムグリーン直方体Cube)
    if (startLineBatchObject_ && startLineVertexCount_ > 0) {
        startLineBatchObject_->GetMaterialData()->color = kStartLineColor;
        startLineBatchObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
        startLineBatchObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        startLineBatchObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        startLineBatchObject_->Update();
        drawer->Draw(startLineBatchObject_.get(), startLineVbView_, startLineVertexCount_, "white", "", "Object3D");
    }

    // 5. GOAL奥ライン (高輝度ゴールド直方体Cube)
    if (goalLineBatchObject_ && goalLineVertexCount_ > 0) {
        goalLineBatchObject_->GetMaterialData()->color = kGoalLineColor;
        goalLineBatchObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
        goalLineBatchObject_->SetScale({ 1.0f, 1.0f, 1.0f });
        goalLineBatchObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
        goalLineBatchObject_->Update();
        drawer->Draw(goalLineBatchObject_.get(), goalLineVbView_, goalLineVertexCount_, "white", "", "Object3D");
    }

    // 6. 接地ホバー枠 (ホバー中マスの四辺直方体Cubeハイライト)
    if (isHoveredValid_ && hoveredGridX_ >= 0 && hoveredGridZ_ >= 0 && hoverBorderBatchObject_) {
        const float tileSize = mapGrid_.tileSize;
        const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
        const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
        const float startX = -totalWidth * kHalfFactor;
        const float startZ = -totalHeight * kHalfFactor;

        float hx0 = startX + static_cast<float>(hoveredGridX_) * tileSize;
        float hx1 = hx0 + tileSize;
        float hz0 = startZ + static_cast<float>(hoveredGridZ_) * tileSize;
        float hz1 = hz0 + tileSize;
        float halfTile = tileSize * kHalfFactor;

        std::vector<VertexData> hoverVertices;
        // 手前辺 (X方向)
        AddQuadBox(hoverVertices, { hx0 + halfTile, kHoverBorderBoxY, hz0 }, { tileSize, kHoverBorderBoxHeight, kHoverBorderThickness });
        // 奥辺 (X方向)
        AddQuadBox(hoverVertices, { hx0 + halfTile, kHoverBorderBoxY, hz1 }, { tileSize, kHoverBorderBoxHeight, kHoverBorderThickness });
        // 左辺 (Z方向)
        AddQuadBox(hoverVertices, { hx0, kHoverBorderBoxY, hz0 + halfTile }, { kHoverBorderThickness, kHoverBorderBoxHeight, tileSize });
        // 右辺 (Z方向)
        AddQuadBox(hoverVertices, { hx1, kHoverBorderBoxY, hz0 + halfTile }, { kHoverBorderThickness, kHoverBorderBoxHeight, tileSize });

        auto engine = EngineResource::GetEngine();
        if (engine && engine->GetDevice()) {
            UploadVertexBuffer(engine->GetDevice(), hoverVertices, hoverBorderVbResource_, hoverBorderVbView_, hoverBorderVertexCount_);
            if (hoverBorderVertexCount_ > 0) {
                hoverBorderBatchObject_->GetMaterialData()->color = (activeTool_ == kTileBuilding) ? kHoverBorderBuildColor : kHoverBorderClearColor;
                hoverBorderBatchObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
                hoverBorderBatchObject_->SetScale({ 1.0f, 1.0f, 1.0f });
                hoverBorderBatchObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
                hoverBorderBatchObject_->Update();
                drawer->Draw(hoverBorderBatchObject_.get(), hoverBorderVbView_, hoverBorderVertexCount_, "white", "", "Object3D");
            }
        }
    }
}

void MapStudioWindow::RenderPreview(ID3D12GraphicsCommandList* cmdList) {
    if (!isInitialized_ || !cmdList || !previewRenderTexture_ || !studioCamera_) return;

    auto engine = EngineResource::GetEngine();
    if (!engine) return;
    auto dxCommon = engine->GetDxCommon();
    if (!dxCommon) return;

    // ビルメッシュに更新があれば再構築
    if (isBuildingMeshDirty_) {
        RebuildBuildingMesh();
    }

    // ★重要 [クラッシュ防止]: SRV ディスクリプタヒープをコマンドリストへ確実にバインド
    ID3D12DescriptorHeap* heaps[] = { dxCommon->GetSrvHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // ★重要: スタジオカメラを一時的にアクティブ化し、定数バッファをGPU更新
    auto cameraMgr = CameraResource::GetCameraManager();
    std::string prevActiveCam = "";
    if (cameraMgr) {
        // 万が一未登録なら即座に自己修復再登録 (シーン切替等の影響を完全遮断)
        if (!cameraMgr->HasCamera(CameraManager::kMapStudioCameraName)) {
            cameraMgr->AddCamera(CameraManager::kMapStudioCameraName, studioCamera_);
        }
        prevActiveCam = cameraMgr->GetActiveCameraName();
        cameraMgr->SetActiveCamera(CameraManager::kMapStudioCameraName, false);
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

    // (A) 外側の拡張ベース土台プレート (暗色プラットフォーム: ステージ境界とのコントラストを劇的に向上)
    if (previewGroundBase_) {
        float baseWidth = totalWidth + kGroundBaseMargin * 2.0f;
        float baseHeight = totalHeight + kGroundBaseMargin * 2.0f;
        previewGroundBase_->SetPosition({ 0.0f, kGroundBaseYOffset, 0.0f });
        previewGroundBase_->SetScale({ baseWidth, kGroundBaseThickness, baseHeight });
        previewGroundBase_->SetRotate({ 0.0f, 0.0f, 0.0f });
        previewGroundBase_->GetMaterialData()->color = kGroundBaseColor;
        previewGroundBase_->Update();
        previewGroundBase_->Draw("white");
    }

    // (B) ステージ作業エリア床面プレート (上面 Y = 0.0f, 明るいグレーで作業エリアを明示)
    if (previewStageGround_) {
        previewStageGround_->SetPosition({ 0.0f, kStageGroundYOffset, 0.0f });
        previewStageGround_->SetScale({ totalWidth, kStageGroundThickness, totalHeight });
        previewStageGround_->SetRotate({ 0.0f, 0.0f, 0.0f });
        previewStageGround_->GetMaterialData()->color = kStageGroundColor;
        previewStageGround_->Update();
        previewStageGround_->Draw("white");
    }

    // (C) グリッド線・ステージ外周境界枠・START/GOALライン・四隅ポスト・ホバー枠の一括描画
    RenderGridAndBorders(cmdList);

    // (D) 配置済みビルの3Dバッチ描画 (全ビルを1回のDrawコールで確実に描画)
    auto drawer = Object3DDrawer::GetInstance();
    if (drawer && previewBuildingCube_ && buildingVertexCount_ > 0) {
        previewBuildingCube_->SetPosition({ 0.0f, 0.0f, 0.0f });
        previewBuildingCube_->SetScale({ 1.0f, 1.0f, 1.0f });
        previewBuildingCube_->SetRotate({ 0.0f, 0.0f, 0.0f });
        previewBuildingCube_->GetMaterialData()->color = mapGrid_.buildingColor;
        previewBuildingCube_->Update();
        drawer->Draw(previewBuildingCube_.get(), buildingVbView_, buildingVertexCount_, "white", "", "Object3D");
    }

    // (E) 3D空間ホバープレビュー (立体的な半透明キューブ)
    if (isHoveredValid_ && hoveredGridX_ >= 0 && hoveredGridZ_ >= 0 && previewHoverCube_) {
        float hPosX = startX + (static_cast<float>(hoveredGridX_) + kHalfFactor) * tileSize;
        float hPosZ = startZ + (static_cast<float>(hoveredGridZ_) + kHalfFactor) * tileSize;
        float hPosY = buildingH * kHalfFactor;

        previewHoverCube_->SetPosition({ hPosX, hPosY, hPosZ });
        previewHoverCube_->SetScale({ tileSize * kPreviewHoverScaleBoost, buildingH * kPreviewHoverScaleBoost, tileSize * kPreviewHoverScaleBoost });
        previewHoverCube_->SetRotate({ 0.0f, 0.0f, 0.0f });
        previewHoverCube_->GetMaterialData()->color = (activeTool_ == kTileBuilding) ? kHoverBuildColor : kHoverClearColor;
        previewHoverCube_->Update();
        previewHoverCube_->Draw("white");
    }

    // 4. レンダーテクスチャをSRV状態へ遷移
    previewRenderTexture_->PostDraw(cmdList);

    // ★重要: メインシーンのカメラへ復元
    if (cameraMgr && !prevActiveCam.empty() && prevActiveCam != CameraManager::kMapStudioCameraName) {
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
    bool isNavigating = isCameraNavigating_ || io.MouseDown[1] || io.MouseDown[2]; // カメラ操作中 (右ドラッグまたは中ドラッグ)

    // 1. 3D地面レイキャスト (カメラ操作中以外で判定)
    if (isViewportHovered_ && !isNavigating) {
        Vector2 mouseLocal = { io.MousePos.x - viewportScreenPos_.x, io.MousePos.y - viewportScreenPos_.y };
        isHoveredValid_ = PerformGroundRaycast(mouseLocal, hoveredGridX_, hoveredGridZ_);
    } else {
        isHoveredValid_ = false;
    }

    // 2. 左クリック/ドラッグによる 3D マス目へのビル配置または消去
    if (isViewportHovered_ && !isNavigating) {
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

        // (A) 操作ガイドバナー (左上 1段目: Blender風操作ガイド)
        ImVec2 banner1Min = ImVec2(viewportScreenPos_.x + 10.0f, viewportScreenPos_.y + 10.0f);
        ImVec2 banner1Max = ImVec2(viewportScreenPos_.x + 670.0f, viewportScreenPos_.y + 34.0f);
        drawList->AddRectFilled(banner1Min, banner1Max, kBannerBg, 4.0f);
        drawList->AddRect(banner1Min, banner1Max, kBannerBorder, 4.0f);

        std::string guideText = "中ドラッグ: 回転(Orbit) | Shift+中ドラッグ: 平行移動(Pan) | ホイール: ズーム | 右ドラッグ+WASD: 移動 | 左: ";
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

        // (C) ホバー中の3Dセル座標表示 (右上: 枠内・枠外を明確に判定表示)
        constexpr ImU32 kOutOfRangeBg = IM_COL32(35, 18, 20, 220);
        constexpr ImU32 kOutOfRangeBorder = IM_COL32(220, 80, 80, 255);
        constexpr ImU32 kOutOfRangeText = IM_COL32(255, 120, 120, 255);

        if (isHoveredValid_) {
            float startX = -totalW * kHalfFactor;
            float startZ = -totalH * kHalfFactor;
            float worldX = startX + (static_cast<float>(hoveredGridX_) + kHalfFactor) * mapGrid_.tileSize;
            float worldZ = startZ + (static_cast<float>(hoveredGridZ_) + kHalfFactor) * mapGrid_.tileSize;

            char coordBuf[96];
            snprintf(coordBuf, sizeof(coordBuf), "3Dグリッド: [%d, %d] (X: %.1fm, Z: %.1fm) [枠内]",
                     hoveredGridX_, hoveredGridZ_, worldX, worldZ);
            ImVec2 coordSize = ImGui::CalcTextSize(coordBuf);
            ImVec2 cMin = ImVec2(viewportScreenPos_.x + vpWidth - coordSize.x - 24.0f, viewportScreenPos_.y + 10.0f);
            ImVec2 cMax = ImVec2(viewportScreenPos_.x + vpWidth - 10.0f, viewportScreenPos_.y + 34.0f);
            drawList->AddRectFilled(cMin, cMax, kBannerBg, 4.0f);
            drawList->AddRect(cMin, cMax, kHighlightColor, 4.0f);
            drawList->AddText(ImVec2(cMin.x + 8.0f, cMin.y + 4.0f), kHighlightColor, coordBuf);
        } else if (isViewportHovered_ && !io.MouseDown[1]) {
            const char* outText = "枠外 (配置不可エリア)";
            ImVec2 outSize = ImGui::CalcTextSize(outText);
            ImVec2 cMin = ImVec2(viewportScreenPos_.x + vpWidth - outSize.x - 24.0f, viewportScreenPos_.y + 10.0f);
            ImVec2 cMax = ImVec2(viewportScreenPos_.x + vpWidth - 10.0f, viewportScreenPos_.y + 34.0f);
            drawList->AddRectFilled(cMin, cMax, kOutOfRangeBg, 4.0f);
            drawList->AddRect(cMin, cMax, kOutOfRangeBorder, 4.0f);
            drawList->AddText(ImVec2(cMin.x + 8.0f, cMin.y + 4.0f), kOutOfRangeText, outText);
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
            isBuildingMeshDirty_ = true;
            RebuildGridAndBorderMeshes();
        }
        if (ImGui::SliderInt("奥行き (Zマス)##GridH", &h, 10, 100)) {
            mapGrid_.Reset(w, h);
            isBuildingMeshDirty_ = true;
            RebuildGridAndBorderMeshes();
        }
        if (ImGui::SliderFloat("マスサイズ (m)##GridTileSize", &ts, 2.0f, 10.0f, "%.1f m")) {
            mapGrid_.tileSize = ts;
            isBuildingMeshDirty_ = true;
            RebuildGridAndBorderMeshes();
        }

        ImGui::TextDisabled("総ワールドサイズ: %.0f m x %.0f m",
                            static_cast<float>(mapGrid_.width) * mapGrid_.tileSize,
                            static_cast<float>(mapGrid_.height) * mapGrid_.tileSize);
        ImGui::Unindent(8.0f);
    }

    // ▼ 2. ビル設定
    if (ImGui::CollapsingHeader("2. ビル設定 (Building Settings)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8.0f);
        if (ImGui::SliderFloat("ビルの高さ (m)##BldgH", &mapGrid_.buildingHeight, 4.0f, 40.0f, "%.1f m")) {
            isBuildingMeshDirty_ = true;
        }
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
            isBuildingMeshDirty_ = true;
            RebuildGridAndBorderMeshes();
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
    isBuildingMeshDirty_ = true;
}

void MapStudioWindow::RemoveBuilding(int x, int z) {
    if (x < 0 || x >= mapGrid_.width || z < 0 || z >= mapGrid_.height) return;
    int prev = mapGrid_.GetTile(x, z);
    if (prev == kTileEmpty) return;

    mapGrid_.SetTile(x, z, kTileEmpty);
    currentBatch_.push_back({ x, z, prev, kTileEmpty });
    isBuildingMeshDirty_ = true;
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
    isBuildingMeshDirty_ = true;
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
    isBuildingMeshDirty_ = true;
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
    isBuildingMeshDirty_ = true;
    RebuildGridAndBorderMeshes();
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
