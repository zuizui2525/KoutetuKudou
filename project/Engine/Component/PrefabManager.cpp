#include "Engine/Component/PrefabManager.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Base/Utils/UUIDUtility.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/Components/AudioSourceComponent.h"
#include "Engine/Component/Components/ParticleEffectComponent.h"
#include "Engine/Component/Components/GPUParticleComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/Components/TextRenderer3DComponent.h"
#include "Engine/Component/Components/RotatorComponent.h"
#include "Engine/Component/Components/SinOscillatorComponent.h"
#include "Engine/Component/Components/BlinkComponent.h"
#include "Engine/Component/Components/CameraOrbitComponent.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Scripts/Player/PlayerControllerScript.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>

namespace {
    // ディレクトリ定数
    const std::string kPrefabsDir = "resources/Prefabs";
    const std::string kPrefabExtension = ".prefab";

    // トリムヘルパー
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    std::string ExtractString(const std::string& block, const std::string& key, const std::string& defaultVal = "") {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        size_t quoteStart = block.find('\"', pos + searchKey.length());
        if (quoteStart == std::string::npos) return defaultVal;
        size_t quoteEnd = block.find('\"', quoteStart + 1);
        if (quoteEnd == std::string::npos) return defaultVal;
        return block.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    float ExtractFloat(const std::string& block, const std::string& key, float defaultVal = 0.0f) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        size_t valStart = pos + searchKey.length();
        while (valStart < block.length() && (block[valStart] == ' ' || block[valStart] == '\t')) {
            ++valStart;
        }
        float val = defaultVal;
        if (sscanf_s(block.c_str() + valStart, "%f", &val) == 1) {
            return val;
        }
        return defaultVal;
    }

    int ExtractInt(const std::string& block, const std::string& key, int defaultVal = 0) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        size_t valStart = pos + searchKey.length();
        while (valStart < block.length() && (block[valStart] == ' ' || block[valStart] == '\t')) {
            ++valStart;
        }
        int val = defaultVal;
        if (sscanf_s(block.c_str() + valStart, "%d", &val) == 1) {
            return val;
        }
        return defaultVal;
    }

    bool ExtractBool(const std::string& block, const std::string& key, bool defaultVal = false) {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return defaultVal;
        size_t valStart = pos + searchKey.length();
        size_t truePos = block.find("true", valStart);
        size_t falsePos = block.find("false", valStart);
        size_t commaPos = block.find_first_of(",}\r\n", valStart);
        if (truePos != std::string::npos && (commaPos == std::string::npos || truePos < commaPos)) {
            return true;
        }
        if (falsePos != std::string::npos && (commaPos == std::string::npos || falsePos < commaPos)) {
            return false;
        }
        return defaultVal;
    }

    std::vector<float> ParseFloatArray(const std::string& block, const std::string& key) {
        std::vector<float> values;
        std::string searchKey = "\"" + key + "\":";
        size_t pos = block.find(searchKey);
        if (pos == std::string::npos) return values;
        size_t arrStart = block.find('[', pos);
        size_t arrEnd = block.find(']', arrStart);
        if (arrStart == std::string::npos || arrEnd == std::string::npos) return values;
        std::string arrContent = block.substr(arrStart + 1, arrEnd - arrStart - 1);
        std::stringstream ss(arrContent);
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::string trimmed = Trim(item);
            if (!trimmed.empty()) {
                try {
                    values.push_back(std::stof(trimmed));
                } catch (...) {
                    values.push_back(0.0f);
                }
            }
        }
        return values;
    }

    Vector2 ExtractVector2(const std::string& block, const std::string& key, const Vector2& defaultVal) {
        auto arr = ParseFloatArray(block, key);
        constexpr size_t kVector2Dimension = 2;
        if (arr.size() >= kVector2Dimension) {
            return { arr[0], arr[1] };
        }
        return defaultVal;
    }

    Vector3 ExtractVector3(const std::string& block, const std::string& key, const Vector3& defaultVal) {
        auto arr = ParseFloatArray(block, key);
        constexpr size_t kVector3Dimension = 3;
        if (arr.size() >= kVector3Dimension) {
            return { arr[0], arr[1], arr[2] };
        }
        return defaultVal;
    }

    Vector4 ExtractVector4(const std::string& block, const std::string& key, const Vector4& defaultVal) {
        auto arr = ParseFloatArray(block, key);
        constexpr size_t kVector4Dimension = 4;
        if (arr.size() >= kVector4Dimension) {
            return { arr[0], arr[1], arr[2], arr[3] };
        }
        return defaultVal;
    }

    std::vector<std::string> ExtractObjectsList(const std::string& json, const std::string& arrayKey) {
        std::vector<std::string> items;
        std::string searchKey = "\"" + arrayKey + "\":";
        size_t startPos = json.find(searchKey);
        if (startPos == std::string::npos) return items;
        size_t arrStart = json.find('[', startPos);
        if (arrStart == std::string::npos) return items;

        size_t pos = arrStart + 1;
        int depth = 0;
        size_t objStart = std::string::npos;

        while (pos < json.length()) {
            char c = json[pos];
            if (c == '{') {
                if (depth == 0) objStart = pos;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && objStart != std::string::npos) {
                    items.push_back(json.substr(objStart, pos - objStart + 1));
                    objStart = std::string::npos;
                }
            } else if (c == ']' && depth == 0) {
                break;
            }
            pos++;
        }
        return items;
    }
}

PrefabManager& PrefabManager::GetInstance() {
    static PrefabManager instance;
    return instance;
}

const std::string& PrefabManager::GetPrefabsDirectory() {
    return kPrefabsDir;
}

void PrefabManager::ClearCache() {
    prefabCache_.clear();
}

std::string PrefabManager::ResolvePrefabPath(const std::string& nameOrPath) const {
    if (nameOrPath.ends_with(kPrefabExtension) || nameOrPath.ends_with(".json")) {
        if (std::filesystem::exists(nameOrPath)) {
            return nameOrPath;
        }
    }

    std::filesystem::path p(nameOrPath);
    if (!p.has_extension()) {
        p.replace_extension(kPrefabExtension);
    }

    std::filesystem::path resolved = std::filesystem::path(kPrefabsDir) / p.filename();
    return resolved.string();
}

bool PrefabManager::SavePrefab(const std::string& filePath, GameObject* gameObject) {
    if (!gameObject) return false;

    std::filesystem::path path(filePath);
    if (path.extension() != kPrefabExtension) {
        path.replace_extension(kPrefabExtension);
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::string serialized = SerializeGameObject(gameObject);

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << serialized;
    ofs.close();

    // キャッシュを更新
    prefabCache_[path.string()] = serialized;
    prefabCache_[path.stem().string()] = serialized;

    return true;
}

std::unique_ptr<GameObject> PrefabManager::Instantiate(
    const std::string& prefabNameOrPath,
    const Vector3& position,
    const Vector3& rotation
) {
    std::string jsonContent = "";

    // 1. メモリキャッシュを検索
    auto it = prefabCache_.find(prefabNameOrPath);
    if (it != prefabCache_.end()) {
        jsonContent = it->second;
    } else {
        // 2. ディスクから読み込み
        std::string path = ResolvePrefabPath(prefabNameOrPath);
        if (!std::filesystem::exists(path)) {
            return nullptr;
        }

        std::ifstream ifs(path);
        if (!ifs.is_open()) return nullptr;

        std::stringstream buffer;
        buffer << ifs.rdbuf();
        jsonContent = buffer.str();

        // キャッシュに保存
        prefabCache_[path] = jsonContent;
        prefabCache_[prefabNameOrPath] = jsonContent;
        prefabCache_[std::filesystem::path(path).stem().string()] = jsonContent;
    }

    // 3. デシリアライズ（新規一意UUIDが自動採番される）
    auto go = DeserializeGameObject(jsonContent);
    if (!go) return nullptr;

    // 4. 指定された座標と回転を適用
    go->SetPosition(position);
    go->SetRotate(rotation);

    return go;
}

std::string PrefabManager::SerializeGameObject(GameObject* gameObject) const {
    if (!gameObject) return "";

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"prefab_name\": \"" << gameObject->GetName() << "\",\n";
    oss << "  \"game_object\": {\n";
    oss << "    \"name\": \"" << gameObject->GetName() << "\",\n";
    oss << "    \"visible\": " << (gameObject->IsVisible() ? "true" : "false") << ",\n";

    // Transform
    const auto& pos = gameObject->GetPosition();
    const auto& rot = gameObject->GetRotate();
    const auto& scl = gameObject->GetScale();
    oss << "    \"transform\": {\n";
    oss << "      \"position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
    oss << "      \"rotation\": [" << rot.x << ", " << rot.y << ", " << rot.z << "],\n";
    oss << "      \"scale\": [" << scl.x << ", " << scl.y << ", " << scl.z << "]\n";
    oss << "    },\n";

    // Components
    oss << "    \"components\": [\n";
    const auto& comps = gameObject->GetComponents();
    bool firstComp = true;

    for (const auto& comp : comps) {
        if (!comp) continue;
        if (!firstComp) oss << ",\n";
        firstComp = false;

        if (auto* mr = dynamic_cast<MeshRendererComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"MeshRenderer\",\n";
            oss << "        \"mesh_type\": " << static_cast<int>(mr->GetMeshType()) << ",\n";
            oss << "        \"texture_key\": \"" << mr->GetTextureKey() << "\",\n";
            oss << "        \"model_key\": \"" << mr->GetModelKey() << "\",\n";
            oss << "        \"env_map_key\": \"" << mr->GetEnvMapKey() << "\",\n";
            const auto& col = mr->GetColor();
            oss << "        \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"lighting_mode\": " << mr->GetLightingMode() << ",\n";
            oss << "        \"shininess\": " << mr->GetShininess() << ",\n";
            oss << "        \"environment_coefficient\": " << mr->GetEnvironmentCoefficient() << ",\n";
            const auto& off = mr->GetOffset();
            oss << "        \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
            oss << "        \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
            oss << "        \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]\n";
            oss << "      }";
        } else if (auto* sr = dynamic_cast<SpriteRendererComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"SpriteRenderer\",\n";
            oss << "        \"shape_type\": " << static_cast<int>(sr->GetShapeType()) << ",\n";
            oss << "        \"texture_key\": \"" << sr->GetTextureKey() << "\",\n";
            const auto& col = sr->GetColor();
            oss << "        \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            const auto& sz = sr->GetSize();
            oss << "        \"size\": [" << sz.x << ", " << sz.y << "],\n";
            oss << "        \"radius\": " << sr->GetRadius() << ",\n";
            oss << "        \"inner_radius\": " << sr->GetInnerRadius() << ",\n";
            const auto& ls = sr->GetLineStart();
            oss << "        \"line_start\": [" << ls.x << ", " << ls.y << "],\n";
            const auto& le = sr->GetLineEnd();
            oss << "        \"line_end\": [" << le.x << ", " << le.y << "],\n";
            oss << "        \"thickness\": " << sr->GetLineThickness() << ",\n";
            const auto& off = sr->GetOffset();
            oss << "        \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
            oss << "        \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
            oss << "        \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]\n";
            oss << "      }";
        } else if (auto* bc = dynamic_cast<BoxColliderComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"BoxCollider\",\n";
            const auto& c = bc->GetCenterOffset();
            oss << "        \"center\": [" << c.x << ", " << c.y << ", " << c.z << "],\n";
            const auto& s = bc->GetSize();
            oss << "        \"size\": [" << s.x << ", " << s.y << ", " << s.z << "],\n";
            oss << "        \"is_trigger\": " << (bc->IsTrigger() ? "true" : "false") << ",\n";
            oss << "        \"show_gizmo\": " << (bc->IsShowDebugGizmo() ? "true" : "false") << ",\n";
            oss << "        \"wire_thickness\": " << bc->GetGizmoWireThickness() << "\n";
            oss << "      }";
        } else if (auto* sc = dynamic_cast<SphereColliderComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"SphereCollider\",\n";
            const auto& c = sc->GetCenterOffset();
            oss << "        \"center\": [" << c.x << ", " << c.y << ", " << c.z << "],\n";
            oss << "        \"radius\": " << sc->GetRadius() << ",\n";
            oss << "        \"is_trigger\": " << (sc->IsTrigger() ? "true" : "false") << ",\n";
            oss << "        \"show_gizmo\": " << (sc->IsShowDebugGizmo() ? "true" : "false") << ",\n";
            oss << "        \"wire_thickness\": " << sc->GetGizmoWireThickness() << "\n";
            oss << "      }";
        } else if (auto* rot = dynamic_cast<RotatorComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"Rotator\",\n";
            const auto& rs = rot->GetRotationSpeed();
            oss << "        \"rotation_speed\": [" << rs.x << ", " << rs.y << ", " << rs.z << "]\n";
            oss << "      }";
        } else if (auto* sinOsc = dynamic_cast<SinOscillatorComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"SinOscillator\",\n";
            oss << "        \"target_property\": " << static_cast<int>(sinOsc->GetTargetType()) << ",\n";
            const auto& amp = sinOsc->GetAmplitude();
            oss << "        \"amplitude\": [" << amp.x << ", " << amp.y << ", " << amp.z << "],\n";
            oss << "        \"frequency\": " << sinOsc->GetFrequency() << ",\n";
            oss << "        \"phase\": " << sinOsc->GetPhase() << "\n";
            oss << "      }";
        } else if (auto* blk = dynamic_cast<BlinkComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"Blink\",\n";
            oss << "        \"blink_speed\": " << blk->GetBlinkSpeed() << ",\n";
            oss << "        \"min_alpha\": " << blk->GetMinAlpha() << ",\n";
            oss << "        \"max_alpha\": " << blk->GetMaxAlpha() << "\n";
            oss << "      }";
        } else if (auto* as = dynamic_cast<AudioSourceComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"AudioSource\",\n";
            oss << "        \"file_path\": \"" << as->GetFilePath() << "\",\n";
            oss << "        \"volume\": " << as->GetVolume() << ",\n";
            oss << "        \"loop\": " << (as->IsLoop() ? "true" : "false") << ",\n";
            oss << "        \"auto_play\": " << (as->IsAutoPlay() ? "true" : "false") << "\n";
            oss << "      }";
        } else if (auto* pe = dynamic_cast<ParticleEffectComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"ParticleEffect\",\n";
            oss << "        \"effect_name\": \"" << pe->GetEffectName() << "\",\n";
            const auto& off = pe->GetOffset();
            oss << "        \"offset\": [" << off.x << ", " << off.y << ", " << off.z << "],\n";
            oss << "        \"loop\": " << (pe->IsLoop() ? "true" : "false") << ",\n";
            oss << "        \"auto_play\": " << (pe->IsAutoPlay() ? "true" : "false") << "\n";
            oss << "      }";
        } else if (auto* gpuComp = dynamic_cast<GPUParticleComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"GPUParticle\",\n";
            oss << "        \"preset_name\": \"" << gpuComp->GetPresetName() << "\",\n";
            oss << "        \"texture_key\": \"" << gpuComp->GetTextureKey() << "\",\n";
            const auto& off = gpuComp->GetOffset();
            oss << "        \"offset\": [" << off.x << ", " << off.y << ", " << off.z << "],\n";
            oss << "        \"auto_spawn\": " << (gpuComp->IsAutoSpawning() ? "true" : "false") << ",\n";
            oss << "        \"blend_mode\": " << static_cast<int>(gpuComp->GetBlendMode()) << ",\n";
            oss << "        \"is_loop\": " << (gpuComp->IsLoop() ? "true" : "false") << ",\n";
            oss << "        \"spawn_rate\": " << gpuComp->GetSpawnRate() << ",\n";
            oss << "        \"burst_interval\": " << gpuComp->GetBurstInterval() << ",\n";
            oss << "        \"burst_count\": " << gpuComp->GetBurstCount() << ",\n";
            oss << "        \"min_lifetime\": " << gpuComp->GetMinLifeTime() << ",\n";
            oss << "        \"max_lifetime\": " << gpuComp->GetMaxLifeTime() << ",\n";
            oss << "        \"gravity\": " << gpuComp->GetGravity() << ",\n";
            const auto& sc = gpuComp->GetStartColor();
            oss << "        \"start_color\": [" << sc.x << ", " << sc.y << ", " << sc.z << ", " << sc.w << "],\n";
            const auto& ec = gpuComp->GetEndColor();
            oss << "        \"end_color\": [" << ec.x << ", " << ec.y << ", " << ec.z << ", " << ec.w << "],\n";
            oss << "        \"firework_text\": \"" << gpuComp->GetFireworkText() << "\",\n";
            oss << "        \"launch_height\": " << gpuComp->GetLaunchHeight() << ",\n";
            oss << "        \"text_scale\": " << gpuComp->GetTextScale() << "\n";
            oss << "      }";
        } else if (auto* cam = dynamic_cast<CameraComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"Camera\",\n";
            oss << "        \"fov\": " << cam->GetFov() << ",\n";
            oss << "        \"near_z\": " << cam->GetNearZ() << ",\n";
            oss << "        \"far_z\": " << cam->GetFarZ() << ",\n";
            oss << "        \"is_main_camera\": " << (cam->IsMainCamera() ? "true" : "false") << ",\n";
            oss << "        \"use_target\": " << (cam->IsUseTarget() ? "true" : "false") << ",\n";
            const auto& tgt = cam->GetTarget();
            oss << "        \"target\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "]\n";
            oss << "      }";
        } else if (auto* light = dynamic_cast<LightComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"Light\",\n";
            std::string lightTypeStr = "Directional";
            if (light->GetLightType() == LightComponent::LightType::Point) lightTypeStr = "Point";
            else if (light->GetLightType() == LightComponent::LightType::Spot) lightTypeStr = "Spot";
            oss << "        \"light_type\": \"" << lightTypeStr << "\",\n";
            const auto& col = light->GetColor();
            oss << "        \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"intensity\": " << light->GetIntensity() << ",\n";
            oss << "        \"range\": " << light->GetRange() << ",\n";
            oss << "        \"decay\": " << light->GetDecay() << ",\n";
            oss << "        \"spot_angle\": " << light->GetSpotAngle() << ",\n";
            oss << "        \"spot_falloff_start\": " << light->GetSpotFalloffStart() << "\n";
            oss << "      }";
        } else if (auto* tr = dynamic_cast<TextRenderer2DComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"TextRenderer2D\",\n";
            oss << "        \"text\": \"" << tr->GetText() << "\",\n";
            oss << "        \"font_name\": \"" << tr->GetFontName() << "\",\n";
            oss << "        \"font_size\": " << tr->GetFontSize() << ",\n";
            const auto& col = tr->GetColor();
            oss << "        \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"alignment\": " << tr->GetAlignment() << ",\n";
            oss << "        \"is_bold\": " << (tr->IsBold() ? "true" : "false") << ",\n";
            oss << "        \"is_italic\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
            oss << "        \"enable_outline\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
            const auto& outCol = tr->GetOutlineColor();
            oss << "        \"outline_color\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
            oss << "        \"outline_width\": " << tr->GetOutlineWidth() << ",\n";
            const auto& off = tr->GetOffset();
            oss << "        \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
            oss << "        \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
            oss << "        \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]\n";
            oss << "      }";
        } else if (auto* tr = dynamic_cast<TextRenderer3DComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"TextRenderer3D\",\n";
            oss << "        \"text\": \"" << tr->GetText() << "\",\n";
            oss << "        \"font_name\": \"" << tr->GetFontName() << "\",\n";
            oss << "        \"font_size\": " << tr->GetFontSize() << ",\n";
            const auto& col = tr->GetColor();
            oss << "        \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"alignment\": " << tr->GetAlignment() << ",\n";
            oss << "        \"billboard_mode\": " << static_cast<int>(tr->GetBillboardMode()) << ",\n";
            oss << "        \"world_scale\": " << tr->GetWorldScale() << ",\n";
            oss << "        \"lighting_mode\": " << tr->GetLightingMode() << ",\n";
            oss << "        \"is_bold\": " << (tr->IsBold() ? "true" : "false") << ",\n";
            oss << "        \"is_italic\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
            oss << "        \"enable_outline\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
            const auto& outCol = tr->GetOutlineColor();
            oss << "        \"outline_color\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
            oss << "        \"outline_width\": " << tr->GetOutlineWidth() << ",\n";
            const auto& off = tr->GetOffset();
            oss << "        \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
            oss << "        \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
            oss << "        \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]\n";
            oss << "      }";
        } else if (auto* co = dynamic_cast<CameraOrbitComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"CameraOrbit\",\n";
            const auto& tgt = co->GetTargetCenter();
            oss << "        \"target_center\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "],\n";
            oss << "        \"radius\": " << co->GetRadius() << ",\n";
            oss << "        \"height\": " << co->GetHeight() << ",\n";
            oss << "        \"orbit_speed\": " << co->GetOrbitSpeed() << "\n";
            oss << "      }";
        } else if (auto* sc = dynamic_cast<ScriptComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"type\": \"ScriptComponent\",\n";
            oss << "        \"script_name\": \"" << sc->GetScriptName() << "\",\n";
            oss << "        \"target_uuid\": \"" << sc->GetTargetUUID() << "\"";
            if (auto* pc = dynamic_cast<PlayerControllerScript*>(sc)) {
                oss << ",\n";
                oss << "        \"move_speed\": " << pc->GetMoveSpeed();
            }
            oss << "\n      }";
        }
    }
    oss << "\n    ]\n";
    oss << "  }\n";
    oss << "}\n";

    return oss.str();
}

std::unique_ptr<GameObject> PrefabManager::DeserializeGameObject(const std::string& jsonContent) const {
    std::string goSearch = "\"game_object\":";
    size_t goPos = jsonContent.find(goSearch);
    std::string goBlock = (goPos != std::string::npos) ? jsonContent.substr(goPos) : jsonContent;

    std::string name = ExtractString(goBlock, "name", "Prefab_Object");
    bool visible = ExtractBool(goBlock, "visible", true);

    auto go = std::make_unique<GameObject>(name);
    // 重要: 新規の一意な UUID を必ず再採番して設定
    go->SetUUID(UUIDUtility::GenerateUUID());
    go->SetVisible(visible);

    // Transform
    std::string transSearch = "\"transform\":";
    size_t transPos = goBlock.find(transSearch);
    if (transPos != std::string::npos) {
        size_t transEnd = goBlock.find('}', transPos);
        std::string transBlock = goBlock.substr(transPos, transEnd - transPos + 1);
        go->SetPosition(ExtractVector3(transBlock, "position", { 0.0f, 0.0f, 0.0f }));
        go->SetRotate(ExtractVector3(transBlock, "rotation", { 0.0f, 0.0f, 0.0f }));
        go->SetScale(ExtractVector3(transBlock, "scale", { 1.0f, 1.0f, 1.0f }));
    }

    // Components
    auto compBlocks = ExtractObjectsList(goBlock, "components");
    for (const auto& compBlock : compBlocks) {
        std::string type = ExtractString(compBlock, "type");

        if (type == "MeshRenderer") {
            auto* mr = go->AddComponent<MeshRendererComponent>();
            int meshVal = ExtractInt(compBlock, "mesh_type", 0);
            mr->SetMeshType(static_cast<MeshRendererComponent::MeshType>(meshVal));
            mr->SetTextureKey(ExtractString(compBlock, "texture_key", "white"));
            mr->SetModelKey(ExtractString(compBlock, "model_key", "cube"));
            mr->SetEnvMapKey(ExtractString(compBlock, "envmap_key", ""));
            mr->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));
            mr->SetLightingMode(ExtractInt(compBlock, "lighting_mode", 1));
            mr->SetShininess(ExtractFloat(compBlock, "shininess", 32.0f));
            mr->SetEnvironmentCoefficient(ExtractFloat(compBlock, "env_coefficient", 0.0f));

            Transform off{};
            off.translate = ExtractVector3(compBlock, "offset_position", { 0.0f, 0.0f, 0.0f });
            off.rotate = ExtractVector3(compBlock, "offset_rotation", { 0.0f, 0.0f, 0.0f });
            off.scale = ExtractVector3(compBlock, "offset_scale", { 1.0f, 1.0f, 1.0f });
            mr->SetOffset(off);
        } else if (type == "SpriteRenderer") {
            auto* sr = go->AddComponent<SpriteRendererComponent>();
            int shapeVal = ExtractInt(compBlock, "shape_type", 0);
            sr->SetShapeType(static_cast<SpriteRendererComponent::ShapeType>(shapeVal));
            sr->SetTextureKey(ExtractString(compBlock, "texture_key", "white"));
            sr->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));
            sr->SetSize(ExtractVector2(compBlock, "size", { 100.0f, 100.0f }));
            sr->SetRadius(ExtractFloat(compBlock, "radius", 50.0f));
            sr->SetInnerRadius(ExtractFloat(compBlock, "inner_radius", 0.0f));
            sr->SetLineStart(ExtractVector2(compBlock, "line_start", { 0.0f, 0.0f }));
            sr->SetLineEnd(ExtractVector2(compBlock, "line_end", { 150.0f, 0.0f }));
            sr->SetLineThickness(ExtractFloat(compBlock, "thickness", 1.0f));

            Transform off{};
            off.translate = ExtractVector3(compBlock, "offset_position", { 0.0f, 0.0f, 0.0f });
            off.rotate = ExtractVector3(compBlock, "offset_rotation", { 0.0f, 0.0f, 0.0f });
            off.scale = ExtractVector3(compBlock, "offset_scale", { 1.0f, 1.0f, 1.0f });
            sr->SetOffset(off);
        } else if (type == "BoxCollider") {
            auto* bc = go->AddComponent<BoxColliderComponent>();
            Vector3 center = ExtractVector3(compBlock, "center_offset", ExtractVector3(compBlock, "center", { 0.0f, 0.0f, 0.0f }));
            bc->SetCenterOffset(center);
            bc->SetSize(ExtractVector3(compBlock, "size", { 1.0f, 1.0f, 1.0f }));
            bc->SetTrigger(ExtractBool(compBlock, "is_trigger", false));
            bc->SetShowDebugGizmo(ExtractBool(compBlock, "show_gizmo", true));
            constexpr float kDefaultWireThickness = 0.03f;
            bc->SetGizmoWireThickness(ExtractFloat(compBlock, "wire_thickness", kDefaultWireThickness));
        } else if (type == "SphereCollider") {
            auto* sc = go->AddComponent<SphereColliderComponent>();
            Vector3 center = ExtractVector3(compBlock, "center_offset", ExtractVector3(compBlock, "center", { 0.0f, 0.0f, 0.0f }));
            sc->SetCenterOffset(center);
            sc->SetRadius(ExtractFloat(compBlock, "radius", 0.5f));
            sc->SetTrigger(ExtractBool(compBlock, "is_trigger", false));
            sc->SetShowDebugGizmo(ExtractBool(compBlock, "show_gizmo", true));
            constexpr float kDefaultWireThickness = 0.03f;
            sc->SetGizmoWireThickness(ExtractFloat(compBlock, "wire_thickness", kDefaultWireThickness));
        } else if (type == "Rotator") {
            auto* rot = go->AddComponent<RotatorComponent>();
            rot->SetRotationSpeed(ExtractVector3(compBlock, "rotation_speed", { 0.0f, 0.5f, 0.0f }));
        } else if (type == "SinOscillator") {
            auto* osc = go->AddComponent<SinOscillatorComponent>();
            int propVal = ExtractInt(compBlock, "target_property", 0);
            osc->SetTargetType(static_cast<SinOscillatorComponent::TargetType>(propVal));
            osc->SetAmplitude(ExtractVector3(compBlock, "amplitude", { 0.0f, 10.0f, 0.0f }));
            osc->SetFrequency(ExtractFloat(compBlock, "frequency", 2.0f));
            osc->SetPhase(ExtractFloat(compBlock, "phase", 0.0f));
        } else if (type == "Blink") {
            auto* blk = go->AddComponent<BlinkComponent>();
            blk->SetBlinkSpeed(ExtractFloat(compBlock, "blink_speed", 3.0f));
            blk->SetMinAlpha(ExtractFloat(compBlock, "min_alpha", 0.15f));
            blk->SetMaxAlpha(ExtractFloat(compBlock, "max_alpha", 1.0f));
        } else if (type == "AudioSource") {
            auto* as = go->AddComponent<AudioSourceComponent>();
            std::string soundPath = ExtractString(compBlock, "file_path", "");
            as->SetFilePath(soundPath);
            as->SetVolume(ExtractFloat(compBlock, "volume", 1.0f));
            as->SetLoop(ExtractBool(compBlock, "loop", false));
            bool autoPlay = ExtractBool(compBlock, "auto_play", false);
            as->SetAutoPlay(autoPlay);
            if (!soundPath.empty()) {
                as->Load(soundPath);
                if (autoPlay) {
                    as->Play();
                }
            }
        } else if (type == "ParticleEffect") {
            auto* pe = go->AddComponent<GPUParticleComponent>();
            pe->SetEffectName(ExtractString(compBlock, "effect_name", "Hit"));
            pe->SetOffset(ExtractVector3(compBlock, "offset", { 0.0f, 0.0f, 0.0f }));
            pe->SetLoop(ExtractBool(compBlock, "loop", false));
            pe->SetAutoPlay(ExtractBool(compBlock, "auto_play", false));
        } else if (type == "GPUParticle") {
            auto* gpuComp = go->AddComponent<GPUParticleComponent>();
            gpuComp->ApplyPreset(ExtractString(compBlock, "preset_name", "BulletTrail"));
            gpuComp->SetTextureKey(ExtractString(compBlock, "texture_key", "circle"));
            gpuComp->SetOffset(ExtractVector3(compBlock, "offset", { 0.0f, 0.0f, 0.0f }));
            gpuComp->SetAutoSpawning(ExtractBool(compBlock, "auto_spawn", true));
            gpuComp->SetSpawnRate(ExtractFloat(compBlock, "spawn_rate", 50.0f));
            int bm = ExtractInt(compBlock, "blend_mode", 1);
            gpuComp->SetBlendMode(static_cast<ParticleBlendMode>(bm));
            bool loop = ExtractBool(compBlock, "is_loop", ExtractBool(compBlock, "auto_spawn", true));
            gpuComp->SetLoop(loop);
            gpuComp->SetBurstInterval(ExtractFloat(compBlock, "burst_interval", 2.0f));
            gpuComp->SetBurstCount(static_cast<uint32_t>(ExtractInt(compBlock, "burst_count", 30)));
            gpuComp->SetMinLifeTime(ExtractFloat(compBlock, "min_lifetime", gpuComp->GetMinLifeTime()));
            gpuComp->SetMaxLifeTime(ExtractFloat(compBlock, "max_lifetime", gpuComp->GetMaxLifeTime()));
            gpuComp->SetGravity(ExtractFloat(compBlock, "gravity", gpuComp->GetGravity()));
            gpuComp->SetStartColor(ExtractVector4(compBlock, "start_color", gpuComp->GetStartColor()));
            gpuComp->SetEndColor(ExtractVector4(compBlock, "end_color", gpuComp->GetEndColor()));
            gpuComp->SetFireworkText(ExtractString(compBlock, "firework_text", "祝"));
            gpuComp->SetLaunchHeight(ExtractFloat(compBlock, "launch_height", 30.0f));
            gpuComp->SetTextScale(ExtractFloat(compBlock, "text_scale", 15.0f));
        } else if (type == "Camera") {
            auto* cam = go->AddComponent<CameraComponent>();
            cam->SetFov(ExtractFloat(compBlock, "fov", 0.45f));
            cam->SetNearZ(ExtractFloat(compBlock, "near_z", 0.1f));
            cam->SetFarZ(ExtractFloat(compBlock, "far_z", 1000.0f));
            cam->SetMainCamera(ExtractBool(compBlock, "is_main_camera", true));
            cam->SetUseTarget(ExtractBool(compBlock, "use_target", false));
            cam->SetTarget(ExtractVector3(compBlock, "target", { 0.0f, 0.0f, 0.0f }));
        } else if (type == "Light") {
            auto* light = go->AddComponent<LightComponent>();
            std::string ltStr = ExtractString(compBlock, "light_type", "Directional");
            if (ltStr == "Point") light->SetLightType(LightComponent::LightType::Point);
            else if (ltStr == "Spot") light->SetLightType(LightComponent::LightType::Spot);
            else light->SetLightType(LightComponent::LightType::Directional);

            light->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));
            light->SetIntensity(ExtractFloat(compBlock, "intensity", 1.0f));
            light->SetRange(ExtractFloat(compBlock, "range", 10.0f));
            light->SetDecay(ExtractFloat(compBlock, "decay", 1.0f));
            light->SetSpotAngle(ExtractFloat(compBlock, "spot_angle", 45.0f));
            light->SetSpotFalloffStart(ExtractFloat(compBlock, "spot_falloff_start", 30.0f));
        } else if (type == "TextRenderer2D") {
            auto* tr = go->AddComponent<TextRenderer2DComponent>();
            tr->SetText(ExtractString(compBlock, "text", "Text"));
            tr->SetFontName(ExtractString(compBlock, "font_name", "Yu Gothic UI"));
            tr->SetFontSize(ExtractFloat(compBlock, "font_size", 32.0f));
            tr->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));
            tr->SetAlignment(ExtractInt(compBlock, "alignment", 0));
            tr->SetBold(ExtractBool(compBlock, "is_bold", false));
            tr->SetItalic(ExtractBool(compBlock, "is_italic", false));
            tr->SetOutlineEnabled(ExtractBool(compBlock, "enable_outline", false));
            tr->SetOutlineColor(ExtractVector4(compBlock, "outline_color", { 0.0f, 0.0f, 0.0f, 1.0f }));
            constexpr float kDefaultOutlineWidth = 2.0f;
            tr->SetOutlineWidth(ExtractFloat(compBlock, "outline_width", kDefaultOutlineWidth));

            Transform off{};
            off.translate = ExtractVector3(compBlock, "offset_position", { 0.0f, 0.0f, 0.0f });
            off.rotate = ExtractVector3(compBlock, "offset_rotation", { 0.0f, 0.0f, 0.0f });
            off.scale = ExtractVector3(compBlock, "offset_scale", { 1.0f, 1.0f, 1.0f });
            tr->SetOffset(off);
        } else if (type == "TextRenderer3D") {
            auto* tr = go->AddComponent<TextRenderer3DComponent>();
            tr->SetText(ExtractString(compBlock, "text", "3D Text"));
            tr->SetFontName(ExtractString(compBlock, "font_name", "Yu Gothic UI"));
            tr->SetFontSize(ExtractFloat(compBlock, "font_size", 48.0f));
            tr->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));
            tr->SetAlignment(ExtractInt(compBlock, "alignment", 1));
            int bmVal = ExtractInt(compBlock, "billboard_mode", static_cast<int>(TextObject3D::BillboardMode::AllAxis));
            tr->SetBillboardMode(static_cast<TextObject3D::BillboardMode>(bmVal));
            tr->SetWorldScale(ExtractFloat(compBlock, "world_scale", 0.02f));
            tr->SetLightingMode(ExtractInt(compBlock, "lighting_mode", 0));
            tr->SetBold(ExtractBool(compBlock, "is_bold", false));
            tr->SetItalic(ExtractBool(compBlock, "is_italic", false));
            tr->SetOutlineEnabled(ExtractBool(compBlock, "enable_outline", false));
            tr->SetOutlineColor(ExtractVector4(compBlock, "outline_color", { 0.0f, 0.0f, 0.0f, 1.0f }));
            constexpr float kDefaultOutlineWidth = 2.0f;
            tr->SetOutlineWidth(ExtractFloat(compBlock, "outline_width", kDefaultOutlineWidth));

            Transform off{};
            off.translate = ExtractVector3(compBlock, "offset_position", { 0.0f, 0.0f, 0.0f });
            off.rotate = ExtractVector3(compBlock, "offset_rotation", { 0.0f, 0.0f, 0.0f });
            off.scale = ExtractVector3(compBlock, "offset_scale", { 1.0f, 1.0f, 1.0f });
            tr->SetOffset(off);
        } else if (type == "CameraOrbit") {
            auto* co = go->AddComponent<CameraOrbitComponent>();
            co->SetTargetCenter(ExtractVector3(compBlock, "target_center", { 0.0f, 0.0f, 0.0f }));
            co->SetRadius(ExtractFloat(compBlock, "radius", 25.0f));
            co->SetHeight(ExtractFloat(compBlock, "height", 4.0f));
            co->SetOrbitSpeed(ExtractFloat(compBlock, "orbit_speed", 0.15f));
        } else if (type == "ScriptComponent") {
            std::string scriptName = ExtractString(compBlock, "script_name", "ScriptComponent");
            std::string targetUUID = ExtractString(compBlock, "target_uuid", "");
            auto script = ScriptRegistry::GetInstance().CreateScript(scriptName);
            if (script) {
                script->SetTargetUUID(targetUUID);
                if (auto* pc = dynamic_cast<PlayerControllerScript*>(script.get())) {
                    constexpr float kDefaultMoveSpeedFallback = 0.15f;
                    float spd = ExtractFloat(compBlock, "move_speed", kDefaultMoveSpeedFallback);
                    pc->SetMoveSpeed(spd);
                }
                go->AddComponentInstance(std::move(script));
            }
        }
    }

    return go;
}
