#include "Scripts/Stage/MapRendererScript.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Debug/DebugEditor.h"
#include "App/Scene/Core/SceneManager.h"
#include "App/Scene/Generic/GenericScene.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace {
    constexpr float kHalfFactor = 0.5f;
    const std::string kCubeModelKey = "cube";
    const std::string kWhiteTextureKey = "white";

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
    ClearMapGeometry();
}

void MapRendererScript::Awake() {
}

void MapRendererScript::Start() {
    if (!isMapLoaded_ && !mapAssetPath_.empty()) {
        LoadMapAsset(mapAssetPath_);
    }
}

void MapRendererScript::OnUpdate() {
}

void MapRendererScript::OnDestroy() {
    ClearMapGeometry();
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
}

void MapRendererScript::BuildMapGeometry() {
    ClearMapGeometry();

    auto* scene = dynamic_cast<GenericScene*>(SceneManager::GetInstance()->GetCurrentScene());
    if (!scene) return;

    auto* owner = GetOwner();
    Vector3 basePos = owner ? owner->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };

    const float tileSize = mapGrid_.tileSize;
    const float buildingH = mapGrid_.buildingHeight;
    const float totalWidth = static_cast<float>(mapGrid_.width) * tileSize;
    const float totalHeight = static_cast<float>(mapGrid_.height) * tileSize;
    const float startX = basePos.x - totalWidth * kHalfFactor;
    const float startZ = basePos.z - totalHeight * kHalfFactor;

    for (int gz = 0; gz < mapGrid_.height; ++gz) {
        for (int gx = 0; gx < mapGrid_.width; ++gx) {
            if (mapGrid_.GetTile(gx, gz) != kTileBuilding) continue;

            const float posX = startX + (static_cast<float>(gx) + kHalfFactor) * tileSize;
            const float posZ = startZ + (static_cast<float>(gz) + kHalfFactor) * tileSize;
            const float posY = basePos.y + buildingH * kHalfFactor;

            std::string objName = "Building_" + std::to_string(gx) + "_" + std::to_string(gz);
            auto bldgObj = std::make_unique<GameObject>(objName);
            bldgObj->SetHideInHierarchy(true);
            bldgObj->SetPosition({ posX, posY, posZ });
            bldgObj->SetScale({ tileSize, buildingH, tileSize });

            // メッシュ描画
            auto* mr = bldgObj->AddComponent<MeshRendererComponent>();
            mr->SetModelKey(kCubeModelKey);
            mr->SetTextureKey(kWhiteTextureKey);
            mr->SetColor(mapGrid_.buildingColor);

            // 当たり判定 (BoxCollider)
            if (generateColliders_) {
                auto* col = bldgObj->AddComponent<BoxColliderComponent>();
                col->SetSize({ tileSize, buildingH, tileSize });
                col->SetCenterOffset({ 0.0f, 0.0f, 0.0f });
            }

            generatedBuildings_.push_back(bldgObj.get());
            scene->AddGameObject(std::move(bldgObj));
        }
    }
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

    ImGui::TextColored(kHeaderColor, "【マップレンダラー (MapRenderer)】");
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
        isMapLoaded_ = false;
    }

    ImGui::Checkbox("コライダー自動生成 (BoxCollider)", &generateColliders_);

    ImGui::Spacing();
    if (isMapLoaded_) {
        ImGui::TextColored(kStatusOkColor, "状態: ロード完了 (%d x %d マス)", mapGrid_.width, mapGrid_.height);
        ImGui::Text("タイルサイズ: %.1f m / ビル高さ: %.1f m", mapGrid_.tileSize, mapGrid_.buildingHeight);
        ImGui::Text("実体化ビル数: %zu 棟", generatedBuildings_.size());
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
