#include "Scripts/Stage/MapRendererScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Debug/DebugEditor.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Drawer/Object3DDrawer.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"
#include "Scripts/Director/GamePhaseControllerScript.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <algorithm>

namespace {
    constexpr float kHalfFactor = 0.5f;
    const std::string kWhiteTextureKey = "white";
    const std::string kPsoName = "Object3D";
    constexpr float kBoundaryWallThickness = 2.0f;
    constexpr float kBoundaryWallHeightBoost = 2.0f;
    constexpr Vector3 kUnitColliderScale = { 1.0f, 1.0f, 1.0f };
    constexpr Vector3 kZeroColliderOffset = { 0.0f, 0.0f, 0.0f };

    // 底面 (-Y) を除外した 5 面ボックス（30 頂点）を生成してバッファへ追加（床面との Z-fighting も根絶）
    void AddBuildingBox(std::vector<VertexData>& vertices, const Vector3& center, const Vector3& size) {
        float hx = size.x * kHalfFactor;
        float hy = size.y * kHalfFactor;
        float hz = size.z * kHalfFactor;

        auto addFace = [&](const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, const Vector3& norm) {
            VertexData v0{ { p0.x, p0.y, p0.z, 1.0f }, { 0.0f, 0.0f }, norm };
            VertexData v1{ { p1.x, p1.y, p1.z, 1.0f }, { 0.0f, 1.0f }, norm };
            VertexData v2{ { p2.x, p2.y, p2.z, 1.0f }, { 1.0f, 0.0f }, norm };
            VertexData v3{ { p3.x, p3.y, p3.z, 1.0f }, { 1.0f, 1.0f }, norm };
            // 時計回り (CW): (左下->左上->右下), (右下->左上->右上)
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
        // 左面 (-X: 左側面)
        addFace({ center.x - hx, center.y - hy, center.z + hz },
                { center.x - hx, center.y + hy, center.z + hz },
                { center.x - hx, center.y - hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z - hz }, { -1.0f, 0.0f, 0.0f });
        // 右面 (+X: 右側面)
        addFace({ center.x + hx, center.y - hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y - hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z + hz }, { 1.0f, 0.0f, 0.0f });
        // 上面 (+Y: 屋根)
        addFace({ center.x - hx, center.y + hy, center.z - hz },
                { center.x - hx, center.y + hy, center.z + hz },
                { center.x + hx, center.y + hy, center.z - hz },
                { center.x + hx, center.y + hy, center.z + hz }, { 0.0f, 1.0f, 0.0f });
    }

    // 結合頂点バッファを GPU リソースへアップロード（遅延解放キューで安全に旧リソースを破棄）
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

    int ExtractInt(const std::string& block, const std::string& key, int defaultVal = 0) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;

        size_t valStart = pos + searchKey.length();
        while (valStart < block.length() && (block[valStart] == ' ' || block[valStart] == '\t')) {
            ++valStart;
        }
        try {
            return std::stoi(block.substr(valStart));
        } catch (...) {
            return defaultVal;
        }
    }

    float ExtractFloat(const std::string& block, const std::string& key, float defaultVal = 0.0f) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;

        size_t valStart = pos + searchKey.length();
        while (valStart < block.length() && (block[valStart] == ' ' || block[valStart] == '\t')) {
            ++valStart;
        }
        try {
            return std::stof(block.substr(valStart));
        } catch (...) {
            return defaultVal;
        }
    }
}

REGISTER_SCRIPT(MapRendererScript);

MapRendererScript::MapRendererScript()
    : ScriptComponent("MapRendererScript") {
}

MapRendererScript::~MapRendererScript() {
    auto it = std::find(sInstances_.begin(), sInstances_.end(), this);
    if (it != sInstances_.end()) {
        sInstances_.erase(it);
    }
    if (sInstance_ == this) {
        sInstance_ = sInstances_.empty() ? nullptr : sInstances_.front();
    }
    ReleaseMeshResources();
    ClearMapGeometry();
    ClearBoundaryColliders();
}

void MapRendererScript::Awake() {
    sInstance_ = this;
    if (std::find(sInstances_.begin(), sInstances_.end(), this) == sInstances_.end()) {
        sInstances_.push_back(this);
    }

    if (!batchCubeObject_) {
        batchCubeObject_ = std::make_unique<CubeObject>();
        batchCubeObject_->SetAutoRegisterHierarchy(false);
        batchCubeObject_->Initialize();
    }

    if (!isMapLoaded_ && !mapAssetPath_.empty()) {
        LoadMapAsset(mapAssetPath_);
    }
}

void MapRendererScript::Start() {
    if (std::find(sInstances_.begin(), sInstances_.end(), this) == sInstances_.end()) {
        sInstances_.push_back(this);
    }
    if (!sInstance_) {
        sInstance_ = this;
    }

    if (!batchCubeObject_) {
        batchCubeObject_ = std::make_unique<CubeObject>();
        batchCubeObject_->SetAutoRegisterHierarchy(false);
        batchCubeObject_->Initialize();
    }

    if (!isMapLoaded_ && !mapAssetPath_.empty()) {
        LoadMapAsset(mapAssetPath_);
    }
}

void MapRendererScript::OnUpdate() {
    if (isMeshDirty_) {
        RebuildBuildingMesh();
    }
}

void MapRendererScript::Draw() {
    if (!owner_ || !owner_->IsVisible() || !isActive_ || !isMapLoaded_) return;

    // 距離カリング判定 (自機が走行中の現在エリア周辺以外は描画スキップしてドローコールとGPU負荷を完全にゼロ化)
    auto* phaseCtrl = GamePhaseControllerScript::GetInstance();
    if (phaseCtrl) {
        int curArea = phaseCtrl->GetCurrentArea();
        float centerZ = owner_->GetPosition().z;
        constexpr float kHalfAreaSpan = kAreaSpanZ * kHalfFactor;
        int myArea = static_cast<int>(std::floor((centerZ + kHalfAreaSpan) / kAreaSpanZ));
        if (myArea < 0) myArea = 0;

        if (std::abs(myArea - curArea) > kMaxVisibleAreaDelta) {
            return; // 遠景エリアは描画を完全にスキップ！
        }
    }

    if (isMeshDirty_) {
        RebuildBuildingMesh();
    }

    auto drawer = Object3DDrawer::GetInstance();
    if (drawer && batchCubeObject_ && buildingVertexCount_ > 0) {
        batchCubeObject_->SetPosition(owner_->GetPosition());
        batchCubeObject_->SetRotate(owner_->GetRotate());
        batchCubeObject_->SetScale(owner_->GetScale());
        batchCubeObject_->GetMaterialData()->color = mapGrid_.buildingColor;
        batchCubeObject_->Update();
        drawer->Draw(batchCubeObject_.get(), buildingVbView_, buildingVertexCount_, kWhiteTextureKey, "", kPsoName);
    }
}

void MapRendererScript::OnDestroy() {
    auto it = std::find(sInstances_.begin(), sInstances_.end(), this);
    if (it != sInstances_.end()) {
        sInstances_.erase(it);
    }
    if (sInstance_ == this) {
        sInstance_ = sInstances_.empty() ? nullptr : sInstances_.front();
    }
    ReleaseMeshResources();
    ClearMapGeometry();
    ClearBoundaryColliders();
}

void MapRendererScript::ReleaseMeshResources() {
    auto deferredMgr = DeferredReleaseManager::GetInstance();
    if (buildingVbResource_ && deferredMgr) {
        deferredMgr->Enqueue(std::move(buildingVbResource_));
    }
    buildingVbView_ = {};
    buildingVertexCount_ = 0;
    batchCubeObject_.reset();
}

bool MapRendererScript::LoadMapAsset(const std::string& filePath) {
    std::string path = filePath.empty() ? mapAssetPath_ : filePath;
    if (path.empty()) return false;

    const std::string ext = ".json";
    if (path.size() < ext.size() || path.substr(path.size() - ext.size()) != ext) {
        path += ext;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::string altPath = "resources/Maps/" + path;
        ifs.open(altPath);
        if (!ifs.is_open()) {
            return false;
        }
        path = altPath;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string jsonStr = buffer.str();
    ifs.close();

    if (!ParseMapJson(jsonStr)) {
        return false;
    }

    mapAssetPath_ = path;
    isMapLoaded_ = true;

    BuildMapGeometry();
    return true;
}

bool MapRendererScript::ParseMapJson(const std::string& jsonString) {
    if (jsonString.empty()) return false;

    int w = kDefaultGridWidth;
    int h = kDefaultGridHeight;
    float tSize = kDefaultTileSize;
    float bHeight = kDefaultBuildingHeight;

    size_t mapPos = jsonString.find("\"map\"");
    if (mapPos != std::string::npos) {
        size_t mapStart = jsonString.find('{', mapPos);
        size_t mapEnd = jsonString.find('}', mapStart);
        if (mapStart != std::string::npos && mapEnd != std::string::npos) {
            std::string mapBlock = jsonString.substr(mapStart, mapEnd - mapStart + 1);
            w = ExtractInt(mapBlock, "width", kDefaultGridWidth);
            h = ExtractInt(mapBlock, "height", kDefaultGridHeight);
            tSize = ExtractFloat(mapBlock, "tileSize", kDefaultTileSize);
            bHeight = ExtractFloat(mapBlock, "buildingHeight", kDefaultBuildingHeight);
        }
    }

    w = (std::max)(kMinGridDimension, (std::min)(kMaxGridDimension, w));
    h = (std::max)(kMinGridDimension, (std::min)(kMaxGridDimension, h));
    tSize = (tSize <= 0.0f) ? kDefaultTileSize : tSize;
    bHeight = (bHeight <= 0.0f) ? kDefaultBuildingHeight : bHeight;

    mapGrid_.tileSize = tSize;
    mapGrid_.buildingHeight = bHeight;
    mapGrid_.Reset(w, h);

    // "buildings" 配列の解析
    size_t bPos = jsonString.find("\"buildings\"");
    if (bPos != std::string::npos) {
        size_t arrayStart = jsonString.find('[', bPos);
        size_t arrayEnd = jsonString.rfind(']');
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos && arrayEnd > arrayStart) {
            size_t curPos = arrayStart + 1;
            while (curPos < arrayEnd) {
                size_t objStart = jsonString.find('{', curPos);
                if (objStart == std::string::npos || objStart >= arrayEnd) break;
                size_t objEnd = jsonString.find('}', objStart);
                if (objEnd == std::string::npos || objEnd > arrayEnd) break;

                std::string block = jsonString.substr(objStart, objEnd - objStart + 1);
                int gx = ExtractInt(block, "x", -1);
                int gz = ExtractInt(block, "z", -1);

                if (gx >= 0 && gx < w && gz >= 0 && gz < h) {
                    mapGrid_.SetTile(gx, gz, kTileBuilding);
                }
                curPos = objEnd + 1;
            }
        }
    }

    return true;
}

void MapRendererScript::ClearMapGeometry() {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (scene) {
        for (auto* obj : generatedBuildings_) {
            if (obj) {
                scene->DestroyGameObject(obj);
            }
        }
    }
    generatedBuildings_.clear();
    ClearBoundaryColliders();
}

void MapRendererScript::BuildMapGeometry() {
    ClearMapGeometry();
    RebuildBuildingMesh();
    if (generateColliders_) {
        BuildBoundaryColliders();
    }
}

void MapRendererScript::RebuildBuildingMesh() {
    auto engine = EngineResource::GetEngine();
    if (!engine || !engine->GetDevice()) return;

    const float tileSize = mapGrid_.tileSize;
    const float buildingH = mapGrid_.buildingHeight;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float startX = -totalWidth * kHalfFactor;
    const float startZ = -totalHeight * kHalfFactor;

    std::vector<VertexData> buildingVertices;

    for (int gz = 0; gz < mapGrid_.height; ++gz) {
        for (int gx = 0; gx < mapGrid_.width; ++gx) {
            if (mapGrid_.GetTile(gx, gz) != kTileBuilding) continue;

            const float posX = startX + (static_cast<float>(gx) + kHalfFactor) * tileSize;
            const float posZ = startZ + (static_cast<float>(gz) + kHalfFactor) * tileSize;
            const float posY = buildingH * kHalfFactor;

            // 底面を除外した 5 面ボックスを生成（床面との Z-fighting も根絶）
            AddBuildingBox(buildingVertices, { posX, posY, posZ }, { tileSize, buildingH, tileSize });
        }
    }

    UploadVertexBuffer(engine->GetDevice(), buildingVertices, buildingVbResource_, buildingVbView_, buildingVertexCount_);
    isMeshDirty_ = false;
}

void MapRendererScript::BuildBoundaryColliders() {
    ClearBoundaryColliders();
    if (!generateColliders_ || !owner_) return;

    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    Vector3 basePos = owner_->GetPosition();
    const float tileSize = mapGrid_.tileSize;
    const float buildingH = mapGrid_.buildingHeight;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float halfWidth = totalWidth * kHalfFactor;

    // 左境界壁コライダー
    auto leftWall = std::make_unique<GameObject>("MapBoundary_Left");
    leftWall->SetHideInHierarchy(true);
    leftWall->SetPosition({ basePos.x - halfWidth - kBoundaryWallThickness * kHalfFactor, basePos.y + buildingH * kHalfFactor, basePos.z });
    leftWall->SetScale({ kBoundaryWallThickness, buildingH * kBoundaryWallHeightBoost, totalHeight });
    auto* colLeft = leftWall->AddComponent<BoxColliderComponent>();
    colLeft->SetSize(kUnitColliderScale);
    colLeft->SetCenterOffset(kZeroColliderOffset);
    colLeft->SetShowDebugGizmo(false);
    boundaryColliders_.push_back(leftWall.get());
    scene->AddGameObject(std::move(leftWall));

    // 右境界壁コライダー
    auto rightWall = std::make_unique<GameObject>("MapBoundary_Right");
    rightWall->SetHideInHierarchy(true);
    rightWall->SetPosition({ basePos.x + halfWidth + kBoundaryWallThickness * kHalfFactor, basePos.y + buildingH * kHalfFactor, basePos.z });
    rightWall->SetScale({ kBoundaryWallThickness, buildingH * kBoundaryWallHeightBoost, totalHeight });
    auto* colRight = rightWall->AddComponent<BoxColliderComponent>();
    colRight->SetSize(kUnitColliderScale);
    colRight->SetCenterOffset(kZeroColliderOffset);
    colRight->SetShowDebugGizmo(false);
    boundaryColliders_.push_back(rightWall.get());
    scene->AddGameObject(std::move(rightWall));
}

void MapRendererScript::ClearBoundaryColliders() {
    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (scene) {
        for (auto* obj : boundaryColliders_) {
            if (obj) {
                scene->DestroyGameObject(obj);
            }
        }
    }
    boundaryColliders_.clear();
}

Vector3 MapRendererScript::GetMapWorldMin() const {
    auto* owner = GetOwner();
    Vector3 basePos = owner ? owner->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    const float tileSize = mapGrid_.tileSize;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    return { basePos.x - totalWidth * kHalfFactor, basePos.y, basePos.z - totalHeight * kHalfFactor };
}

Vector3 MapRendererScript::GetMapWorldMax() const {
    auto* owner = GetOwner();
    Vector3 basePos = owner ? owner->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    const float tileSize = mapGrid_.tileSize;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    return { basePos.x + totalWidth * kHalfFactor, basePos.y + mapGrid_.buildingHeight, basePos.z + totalHeight * kHalfFactor };
}

void MapRendererScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.3f, 0.8f, 1.0f, 1.0f };
    constexpr ImVec4 kStatusOkColor = { 0.2f, 1.0f, 0.3f, 1.0f };
    constexpr ImVec4 kStatusErrColor = { 1.0f, 0.3f, 0.3f, 1.0f };

    ImGui::TextColored(kHeaderColor, "【マップレンダラー (MapRenderer - バッチ高速化版)】");
    ImGui::Separator();

    char pathBuf[kMaxPathBufferLength] = {};
    strcpy_s(pathBuf, mapAssetPath_.c_str());
    if (ImGui::InputText("マップアセット##MapAssetPath", pathBuf, sizeof(pathBuf))) {
        mapAssetPath_ = pathBuf;
    }

    if (ImGui::Button("再読込 (Reload)")) {
        LoadMapAsset(mapAssetPath_);
    }
    ImGui::SameLine();
    if (ImGui::Button("クリア (Clear)")) {
        ClearMapGeometry();
        ReleaseMeshResources();
        isMapLoaded_ = false;
    }

    if (ImGui::Checkbox("境界コライダー自動生成 (BoxCollider)", &generateColliders_)) {
        if (generateColliders_) {
            BuildBoundaryColliders();
        } else {
            ClearBoundaryColliders();
        }
    }

    ImGui::Spacing();
    if (isMapLoaded_) {
        ImGui::TextColored(kStatusOkColor, "状態: ロード完了 (%d x %d マス)", mapGrid_.width, mapGrid_.height);
        ImGui::Text("タイルサイズ: %.1f m / ビル高さ: %.1f m", mapGrid_.tileSize, mapGrid_.buildingHeight);
        ImGui::Text("結合バッチ頂点数: %u (Drawコール: 1回)", buildingVertexCount_);
        ImGui::Text("外枠コライダー数: %zu 個 (軽量化済み)", boundaryColliders_.size());
    } else {
        ImGui::TextColored(kStatusErrColor, "状態: 未ロード");
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Map Studio で編集... (Edit in Studio)", ImVec2(-1.0f, 28.0f))) {
        if (auto* editor = DebugEditor::GetInstance()) {
            editor->OpenMapStudio(mapAssetPath_);
        }
    }
#endif
}
