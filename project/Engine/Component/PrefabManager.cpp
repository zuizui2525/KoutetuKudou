#include "Engine/Component/PrefabManager.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Base/Utils/UUIDUtility.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/Components/AudioSourceComponent.h"
#include "Engine/Component/Components/ParticleEffectComponent.h"
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

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>

namespace {
    // ディレクトリ定数
    const std::string kPrefabsDir = "resources/Prefabs";
    const std::string kPrefabExtension = ".prefab";

    // JSONキー定数（マジックストリング排除）
    constexpr const char* kKeyPrefabName = "prefab_name";
    constexpr const char* kKeyGameObject = "game_object";
    constexpr const char* kKeyName = "name";
    constexpr const char* kKeyVisible = "visible";
    constexpr const char* kKeyTransform = "transform";
    constexpr const char* kKeyPosition = "position";
    constexpr const char* kKeyRotation = "rotation";
    constexpr const char* kKeyScale = "scale";
    constexpr const char* kKeyComponents = "components";
    constexpr const char* kKeyType = "type";

    // 各コンポーネント用キー定数
    constexpr const char* kTypeMeshRenderer = "MeshRenderer";
    constexpr const char* kTypeSpriteRenderer = "SpriteRenderer";
    constexpr const char* kTypeBoxCollider = "BoxCollider";
    constexpr const char* kTypeSphereCollider = "SphereCollider";
    constexpr const char* kTypeAudioSource = "AudioSource";
    constexpr const char* kTypeParticleEffect = "ParticleEffect";
    constexpr const char* kTypeCamera = "Camera";
    constexpr const char* kTypeLight = "Light";
    constexpr const char* kTypeTextRenderer2D = "TextRenderer2D";
    constexpr const char* kTypeTextRenderer3D = "TextRenderer3D";
    constexpr const char* kTypeRotator = "Rotator";
    constexpr const char* kTypeSinOscillator = "SinOscillator";
    constexpr const char* kTypeBlink = "Blink";
    constexpr const char* kTypeCameraOrbit = "CameraOrbit";
    constexpr const char* kTypeScriptComponent = "ScriptComponent";

    constexpr const char* kKeyMeshType = "mesh_type";
    constexpr const char* kKeyTextureKey = "texture_key";
    constexpr const char* kKeyModelKey = "model_key";
    constexpr const char* kKeyEnvMapKey = "env_map_key";
    constexpr const char* kKeyColor = "color";
    constexpr const char* kKeyLightingMode = "lighting_mode";
    constexpr const char* kKeyShininess = "shininess";
    constexpr const char* kKeyEnvironmentCoefficient = "environment_coefficient";

    constexpr const char* kKeyShapeType = "shape_type";
    constexpr const char* kKeySize = "size";
    constexpr const char* kKeyRadius = "radius";
    constexpr const char* kKeyInnerRadius = "inner_radius";
    constexpr const char* kKeyLineStart = "line_start";
    constexpr const char* kKeyLineEnd = "line_end";
    constexpr const char* kKeyThickness = "thickness";

    constexpr const char* kKeyCenter = "center";
    constexpr const char* kKeyIsTrigger = "is_trigger";
    constexpr const char* kKeyShowGizmo = "show_gizmo";

    constexpr const char* kKeyFilePath = "file_path";
    constexpr const char* kKeyVolume = "volume";
    constexpr const char* kKeyLoop = "loop";
    constexpr const char* kKeyAutoPlay = "auto_play";

    constexpr const char* kKeyEffectName = "effect_name";
    constexpr const char* kKeyOffset = "offset";

    constexpr const char* kKeyFov = "fov";
    constexpr const char* kKeyNearZ = "near_z";
    constexpr const char* kKeyFarZ = "far_z";
    constexpr const char* kKeyIsMainCamera = "is_main_camera";
    constexpr const char* kKeyUseTarget = "use_target";
    constexpr const char* kKeyTarget = "target";

    constexpr const char* kKeyLightType = "light_type";
    constexpr const char* kKeyIntensity = "intensity";
    constexpr const char* kKeyRange = "range";
    constexpr const char* kKeyDecay = "decay";
    constexpr const char* kKeySpotAngle = "spot_angle";
    constexpr const char* kKeySpotFalloffStart = "spot_falloff_start";

    constexpr const char* kKeyText = "text";
    constexpr const char* kKeyFontName = "font_name";
    constexpr const char* kKeyFontSize = "font_size";
    constexpr const char* kKeyAlignment = "alignment";
    constexpr const char* kKeyIsBold = "is_bold";
    constexpr const char* kKeyIsItalic = "is_italic";
    constexpr const char* kKeyBillboardMode = "billboard_mode";
    constexpr const char* kKeyWorldScale = "world_scale";
    constexpr const char* kKeyEnableOutline = "enable_outline";
    constexpr const char* kKeyOutlineColor = "outline_color";
    constexpr const char* kKeyOutlineWidth = "outline_width";

    constexpr const char* kKeyRotationSpeed = "rotation_speed";
    constexpr const char* kKeyTargetProperty = "target_property";
    constexpr const char* kKeyAmplitude = "amplitude";
    constexpr const char* kKeyFrequency = "frequency";
    constexpr const char* kKeyPhase = "phase";
    constexpr const char* kKeyBlinkSpeed = "blink_speed";
    constexpr const char* kKeyMinAlpha = "min_alpha";
    constexpr const char* kKeyMaxAlpha = "max_alpha";
    constexpr const char* kKeyTargetCenter = "target_center";
    constexpr const char* kKeyHeight = "height";
    constexpr const char* kKeyOrbitSpeed = "orbit_speed";

    constexpr const char* kKeyOffsetPosition = "offset_position";
    constexpr const char* kKeyOffsetRotation = "offset_rotation";
    constexpr const char* kKeyOffsetScale = "offset_scale";

    constexpr const char* kKeyScriptName = "script_name";
    constexpr const char* kKeyTargetUUID = "target_uuid";

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
    oss << "  \"" << kKeyPrefabName << "\": \"" << gameObject->GetName() << "\",\n";
    oss << "  \"" << kKeyGameObject << "\": {\n";
    oss << "    \"" << kKeyName << "\": \"" << gameObject->GetName() << "\",\n";
    oss << "    \"" << kKeyVisible << "\": " << (gameObject->IsVisible() ? "true" : "false") << ",\n";

    // Transform
    const auto& pos = gameObject->GetPosition();
    const auto& rot = gameObject->GetRotate();
    const auto& scl = gameObject->GetScale();
    oss << "    \"" << kKeyTransform << "\": {\n";
    oss << "      \"" << kKeyPosition << "\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
    oss << "      \"" << kKeyRotation << "\": [" << rot.x << ", " << rot.y << ", " << rot.z << "],\n";
    oss << "      \"" << kKeyScale << "\": [" << scl.x << ", " << scl.y << ", " << scl.z << "]\n";
    oss << "    },\n";

    // Components
    oss << "    \"" << kKeyComponents << "\": [\n";
    const auto& comps = gameObject->GetComponents();
    bool firstComp = true;

    for (const auto& comp : comps) {
        if (!comp) continue;
        if (!firstComp) oss << ",\n";
        firstComp = false;

        if (auto* mr = dynamic_cast<MeshRendererComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeMeshRenderer << "\",\n";
            oss << "        \"" << kKeyMeshType << "\": " << static_cast<int>(mr->GetMeshType()) << ",\n";
            oss << "        \"" << kKeyTextureKey << "\": \"" << mr->GetTextureKey() << "\",\n";
            oss << "        \"" << kKeyModelKey << "\": \"" << mr->GetModelKey() << "\",\n";
            oss << "        \"" << kKeyEnvMapKey << "\": \"" << mr->GetEnvMapKey() << "\",\n";
            const auto& col = mr->GetColor();
            oss << "        \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"" << kKeyLightingMode << "\": " << mr->GetLightingMode() << ",\n";
            oss << "        \"" << kKeyShininess << "\": " << mr->GetShininess() << ",\n";
            oss << "        \"" << kKeyEnvironmentCoefficient << "\": " << mr->GetEnvironmentCoefficient() << "\n";
            oss << "      }";
        } else if (auto* sr = dynamic_cast<SpriteRendererComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeSpriteRenderer << "\",\n";
            oss << "        \"" << kKeyShapeType << "\": " << static_cast<int>(sr->GetShapeType()) << ",\n";
            oss << "        \"" << kKeyTextureKey << "\": \"" << sr->GetTextureKey() << "\",\n";
            const auto& col = sr->GetColor();
            oss << "        \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            const auto& sz = sr->GetSize();
            oss << "        \"" << kKeySize << "\": [" << sz.x << ", " << sz.y << "],\n";
            oss << "        \"" << kKeyRadius << "\": " << sr->GetRadius() << ",\n";
            oss << "        \"" << kKeyInnerRadius << "\": " << sr->GetInnerRadius() << ",\n";
            const auto& ls = sr->GetLineStart();
            oss << "        \"" << kKeyLineStart << "\": [" << ls.x << ", " << ls.y << "],\n";
            const auto& le = sr->GetLineEnd();
            oss << "        \"" << kKeyLineEnd << "\": [" << le.x << ", " << le.y << "],\n";
            oss << "        \"" << kKeyThickness << "\": " << sr->GetLineThickness() << "\n";
            oss << "      }";
        } else if (auto* bc = dynamic_cast<BoxColliderComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeBoxCollider << "\",\n";
            const auto& c = bc->GetCenterOffset();
            oss << "        \"" << kKeyCenter << "\": [" << c.x << ", " << c.y << ", " << c.z << "],\n";
            const auto& s = bc->GetSize();
            oss << "        \"" << kKeySize << "\": [" << s.x << ", " << s.y << ", " << s.z << "],\n";
            oss << "        \"" << kKeyIsTrigger << "\": " << (bc->IsTrigger() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyShowGizmo << "\": " << (bc->IsShowDebugGizmo() ? "true" : "false") << "\n";
            oss << "      }";
        } else if (auto* sc = dynamic_cast<SphereColliderComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeSphereCollider << "\",\n";
            const auto& c = sc->GetCenterOffset();
            oss << "        \"" << kKeyCenter << "\": [" << c.x << ", " << c.y << ", " << c.z << "],\n";
            oss << "        \"" << kKeyRadius << "\": " << sc->GetRadius() << ",\n";
            oss << "        \"" << kKeyIsTrigger << "\": " << (sc->IsTrigger() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyShowGizmo << "\": " << (sc->IsShowDebugGizmo() ? "true" : "false") << "\n";
            oss << "      }";
        } else if (auto* script = dynamic_cast<ScriptComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeScriptComponent << "\",\n";
            oss << "        \"" << kKeyScriptName << "\": \"" << script->GetScriptName() << "\",\n";
            oss << "        \"" << kKeyTargetUUID << "\": \"" << script->GetTargetUUID() << "\"\n";
            oss << "      }";
        } else if (auto* rot = dynamic_cast<RotatorComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeRotator << "\",\n";
            const auto& rs = rot->GetRotationSpeed();
            oss << "        \"" << kKeyRotationSpeed << "\": [" << rs.x << ", " << rs.y << ", " << rs.z << "]\n";
            oss << "      }";
        } else if (auto* sinOsc = dynamic_cast<SinOscillatorComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeSinOscillator << "\",\n";
            oss << "        \"" << kKeyTargetProperty << "\": " << static_cast<int>(sinOsc->GetTargetType()) << ",\n";
            const auto& amp = sinOsc->GetAmplitude();
            oss << "        \"" << kKeyAmplitude << "\": [" << amp.x << ", " << amp.y << ", " << amp.z << "],\n";
            oss << "        \"" << kKeyFrequency << "\": " << sinOsc->GetFrequency() << ",\n";
            oss << "        \"" << kKeyPhase << "\": " << sinOsc->GetPhase() << "\n";
            oss << "      }";
        } else if (auto* blk = dynamic_cast<BlinkComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeBlink << "\",\n";
            oss << "        \"" << kKeyBlinkSpeed << "\": " << blk->GetBlinkSpeed() << ",\n";
            oss << "        \"" << kKeyMinAlpha << "\": " << blk->GetMinAlpha() << ",\n";
            oss << "        \"" << kKeyMaxAlpha << "\": " << blk->GetMaxAlpha() << "\n";
            oss << "      }";
        } else if (auto* as = dynamic_cast<AudioSourceComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeAudioSource << "\",\n";
            oss << "        \"" << kKeyFilePath << "\": \"" << as->GetFilePath() << "\",\n";
            oss << "        \"" << kKeyVolume << "\": " << as->GetVolume() << ",\n";
            oss << "        \"" << kKeyLoop << "\": " << (as->IsLoop() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyAutoPlay << "\": " << (as->IsAutoPlay() ? "true" : "false") << "\n";
            oss << "      }";
        } else if (auto* pe = dynamic_cast<ParticleEffectComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeParticleEffect << "\",\n";
            oss << "        \"" << kKeyEffectName << "\": \"" << pe->GetEffectName() << "\",\n";
            const auto& off = pe->GetOffset();
            oss << "        \"" << kKeyOffset << "\": [" << off.x << ", " << off.y << ", " << off.z << "],\n";
            oss << "        \"" << kKeyLoop << "\": " << (pe->IsLoop() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyAutoPlay << "\": " << (pe->IsAutoPlay() ? "true" : "false") << "\n";
            oss << "      }";
        } else if (auto* cam = dynamic_cast<CameraComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeCamera << "\",\n";
            oss << "        \"" << kKeyFov << "\": " << cam->GetFov() << ",\n";
            oss << "        \"" << kKeyNearZ << "\": " << cam->GetNearZ() << ",\n";
            oss << "        \"" << kKeyFarZ << "\": " << cam->GetFarZ() << ",\n";
            oss << "        \"" << kKeyIsMainCamera << "\": " << (cam->IsMainCamera() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyUseTarget << "\": " << (cam->IsUseTarget() ? "true" : "false") << ",\n";
            const auto& tgt = cam->GetTarget();
            oss << "        \"" << kKeyTarget << "\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "]\n";
            oss << "      }";
        } else if (auto* light = dynamic_cast<LightComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeLight << "\",\n";
            std::string lightTypeStr = "Directional";
            if (light->GetLightType() == LightComponent::LightType::Point) lightTypeStr = "Point";
            else if (light->GetLightType() == LightComponent::LightType::Spot) lightTypeStr = "Spot";
            oss << "        \"" << kKeyLightType << "\": \"" << lightTypeStr << "\",\n";
            const auto& col = light->GetColor();
            oss << "        \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"" << kKeyIntensity << "\": " << light->GetIntensity() << ",\n";
            oss << "        \"" << kKeyRange << "\": " << light->GetRange() << ",\n";
            oss << "        \"" << kKeyDecay << "\": " << light->GetDecay() << ",\n";
            oss << "        \"" << kKeySpotAngle << "\": " << light->GetSpotAngle() << ",\n";
            oss << "        \"" << kKeySpotFalloffStart << "\": " << light->GetSpotFalloffStart() << "\n";
            oss << "      }";
        } else if (auto* tr = dynamic_cast<TextRenderer2DComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeTextRenderer2D << "\",\n";
            oss << "        \"" << kKeyText << "\": \"" << tr->GetText() << "\",\n";
            oss << "        \"" << kKeyFontName << "\": \"" << tr->GetFontName() << "\",\n";
            oss << "        \"" << kKeyFontSize << "\": " << tr->GetFontSize() << ",\n";
            const auto& col = tr->GetColor();
            oss << "        \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"" << kKeyAlignment << "\": " << tr->GetAlignment() << ",\n";
            oss << "        \"" << kKeyIsBold << "\": " << (tr->IsBold() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyIsItalic << "\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyEnableOutline << "\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
            const auto& outCol = tr->GetOutlineColor();
            oss << "        \"" << kKeyOutlineColor << "\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
            oss << "        \"" << kKeyOutlineWidth << "\": " << tr->GetOutlineWidth() << ",\n";
            const auto& off = tr->GetOffset();
            oss << "        \"" << kKeyOffsetPosition << "\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
            oss << "        \"" << kKeyOffsetRotation << "\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
            oss << "        \"" << kKeyOffsetScale << "\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]\n";
            oss << "      }";
        } else if (auto* tr = dynamic_cast<TextRenderer3DComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeTextRenderer3D << "\",\n";
            oss << "        \"" << kKeyText << "\": \"" << tr->GetText() << "\",\n";
            oss << "        \"" << kKeyFontName << "\": \"" << tr->GetFontName() << "\",\n";
            oss << "        \"" << kKeyFontSize << "\": " << tr->GetFontSize() << ",\n";
            const auto& col = tr->GetColor();
            oss << "        \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
            oss << "        \"" << kKeyAlignment << "\": " << tr->GetAlignment() << ",\n";
            oss << "        \"" << kKeyBillboardMode << "\": " << static_cast<int>(tr->GetBillboardMode()) << ",\n";
            oss << "        \"" << kKeyWorldScale << "\": " << tr->GetWorldScale() << ",\n";
            oss << "        \"" << kKeyLightingMode << "\": " << tr->GetLightingMode() << ",\n";
            oss << "        \"" << kKeyIsBold << "\": " << (tr->IsBold() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyIsItalic << "\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
            oss << "        \"" << kKeyEnableOutline << "\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
            const auto& outCol = tr->GetOutlineColor();
            oss << "        \"" << kKeyOutlineColor << "\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
            oss << "        \"" << kKeyOutlineWidth << "\": " << tr->GetOutlineWidth() << ",\n";
            const auto& off = tr->GetOffset();
            oss << "        \"" << kKeyOffsetPosition << "\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
            oss << "        \"" << kKeyOffsetRotation << "\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
            oss << "        \"" << kKeyOffsetScale << "\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]\n";
            oss << "      }";
        } else if (auto* co = dynamic_cast<CameraOrbitComponent*>(comp.get())) {
            oss << "      {\n";
            oss << "        \"" << kKeyType << "\": \"" << kTypeCameraOrbit << "\",\n";
            const auto& tgt = co->GetTargetCenter();
            oss << "        \"" << kKeyTargetCenter << "\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "],\n";
            oss << "        \"" << kKeyRadius << "\": " << co->GetRadius() << ",\n";
            oss << "        \"" << kKeyHeight << "\": " << co->GetHeight() << ",\n";
            oss << "        \"" << kKeyOrbitSpeed << "\": " << co->GetOrbitSpeed() << "\n";
            oss << "      }";
        }
    }
    oss << "\n    ]\n";
    oss << "  }\n";
    oss << "}\n";

    return oss.str();
}

std::unique_ptr<GameObject> PrefabManager::DeserializeGameObject(const std::string& jsonContent) const {
    std::string goSearch = "\"" + std::string(kKeyGameObject) + "\":";
    size_t goPos = jsonContent.find(goSearch);
    std::string goBlock = (goPos != std::string::npos) ? jsonContent.substr(goPos) : jsonContent;

    std::string name = ExtractString(goBlock, kKeyName, "Prefab_Object");
    bool visible = ExtractBool(goBlock, kKeyVisible, true);

    auto go = std::make_unique<GameObject>(name);
    // 重要: 新規の一意な UUID を必ず再採番して設定
    go->SetUUID(UUIDUtility::GenerateUUID());
    go->SetVisible(visible);

    // Transform
    std::string transSearch = "\"" + std::string(kKeyTransform) + "\":";
    size_t transPos = goBlock.find(transSearch);
    if (transPos != std::string::npos) {
        size_t transEnd = goBlock.find('}', transPos);
        std::string transBlock = goBlock.substr(transPos, transEnd - transPos + 1);
        go->SetPosition(ExtractVector3(transBlock, kKeyPosition, { 0.0f, 0.0f, 0.0f }));
        go->SetRotate(ExtractVector3(transBlock, kKeyRotation, { 0.0f, 0.0f, 0.0f }));
        go->SetScale(ExtractVector3(transBlock, kKeyScale, { 1.0f, 1.0f, 1.0f }));
    }

    // Components
    auto compBlocks = ExtractObjectsList(goBlock, kKeyComponents);
    for (const auto& compBlock : compBlocks) {
        std::string type = ExtractString(compBlock, kKeyType);

        if (type == kTypeMeshRenderer) {
            auto* mr = go->AddComponent<MeshRendererComponent>();
            int meshVal = ExtractInt(compBlock, kKeyMeshType, 0);
            mr->SetMeshType(static_cast<MeshRendererComponent::MeshType>(meshVal));
            mr->SetTextureKey(ExtractString(compBlock, kKeyTextureKey, "white"));
            mr->SetModelKey(ExtractString(compBlock, kKeyModelKey, "cube"));
            mr->SetEnvMapKey(ExtractString(compBlock, kKeyEnvMapKey, ""));
            mr->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));
            mr->SetLightingMode(ExtractInt(compBlock, kKeyLightingMode, 1));
            mr->SetShininess(ExtractFloat(compBlock, kKeyShininess, 32.0f));
            mr->SetEnvironmentCoefficient(ExtractFloat(compBlock, kKeyEnvironmentCoefficient, 0.0f));
        } else if (type == kTypeSpriteRenderer) {
            auto* sr = go->AddComponent<SpriteRendererComponent>();
            int shapeVal = ExtractInt(compBlock, kKeyShapeType, 0);
            sr->SetShapeType(static_cast<SpriteRendererComponent::ShapeType>(shapeVal));
            sr->SetTextureKey(ExtractString(compBlock, kKeyTextureKey, "white"));
            sr->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));
            sr->SetSize(ExtractVector2(compBlock, kKeySize, { 100.0f, 100.0f }));
            sr->SetRadius(ExtractFloat(compBlock, kKeyRadius, 50.0f));
            sr->SetInnerRadius(ExtractFloat(compBlock, kKeyInnerRadius, 0.0f));
            sr->SetLineStart(ExtractVector2(compBlock, kKeyLineStart, { 0.0f, 0.0f }));
            sr->SetLineEnd(ExtractVector2(compBlock, kKeyLineEnd, { 150.0f, 0.0f }));
            sr->SetLineThickness(ExtractFloat(compBlock, kKeyThickness, 1.0f));
        } else if (type == kTypeBoxCollider) {
            auto* bc = go->AddComponent<BoxColliderComponent>();
            bc->SetCenterOffset(ExtractVector3(compBlock, kKeyCenter, { 0.0f, 0.0f, 0.0f }));
            bc->SetSize(ExtractVector3(compBlock, kKeySize, { 1.0f, 1.0f, 1.0f }));
            bc->SetTrigger(ExtractBool(compBlock, kKeyIsTrigger, false));
            bc->SetShowDebugGizmo(ExtractBool(compBlock, kKeyShowGizmo, true));
        } else if (type == kTypeSphereCollider) {
            auto* sc = go->AddComponent<SphereColliderComponent>();
            sc->SetCenterOffset(ExtractVector3(compBlock, kKeyCenter, { 0.0f, 0.0f, 0.0f }));
            sc->SetRadius(ExtractFloat(compBlock, kKeyRadius, 0.5f));
            sc->SetTrigger(ExtractBool(compBlock, kKeyIsTrigger, false));
            sc->SetShowDebugGizmo(ExtractBool(compBlock, kKeyShowGizmo, true));
        } else if (type == kTypeRotator) {
            auto* rot = go->AddComponent<RotatorComponent>();
            rot->SetRotationSpeed(ExtractVector3(compBlock, kKeyRotationSpeed, { 0.0f, 0.5f, 0.0f }));
        } else if (type == kTypeSinOscillator) {
            auto* osc = go->AddComponent<SinOscillatorComponent>();
            int propVal = ExtractInt(compBlock, kKeyTargetProperty, 0);
            osc->SetTargetType(static_cast<SinOscillatorComponent::TargetType>(propVal));
            osc->SetAmplitude(ExtractVector3(compBlock, kKeyAmplitude, { 0.0f, 10.0f, 0.0f }));
            osc->SetFrequency(ExtractFloat(compBlock, kKeyFrequency, 2.0f));
            osc->SetPhase(ExtractFloat(compBlock, kKeyPhase, 0.0f));
        } else if (type == kTypeBlink) {
            auto* blk = go->AddComponent<BlinkComponent>();
            blk->SetBlinkSpeed(ExtractFloat(compBlock, kKeyBlinkSpeed, 3.0f));
            blk->SetMinAlpha(ExtractFloat(compBlock, kKeyMinAlpha, 0.15f));
            blk->SetMaxAlpha(ExtractFloat(compBlock, kKeyMaxAlpha, 1.0f));
        } else if (type == kTypeAudioSource) {
            auto* as = go->AddComponent<AudioSourceComponent>();
            std::string soundPath = ExtractString(compBlock, kKeyFilePath, "");
            as->SetFilePath(soundPath);
            as->SetVolume(ExtractFloat(compBlock, kKeyVolume, 1.0f));
            as->SetLoop(ExtractBool(compBlock, kKeyLoop, false));
            bool autoPlay = ExtractBool(compBlock, kKeyAutoPlay, false);
            as->SetAutoPlay(autoPlay);
            if (!soundPath.empty()) {
                as->Load(soundPath);
                if (autoPlay) {
                    as->Play();
                }
            }
        } else if (type == kTypeParticleEffect) {
            auto* pe = go->AddComponent<ParticleEffectComponent>();
            pe->SetEffectName(ExtractString(compBlock, kKeyEffectName, "Hit"));
            pe->SetOffset(ExtractVector3(compBlock, kKeyOffset, { 0.0f, 0.0f, 0.0f }));
            pe->SetLoop(ExtractBool(compBlock, kKeyLoop, false));
            pe->SetAutoPlay(ExtractBool(compBlock, kKeyAutoPlay, false));
        } else if (type == kTypeCamera) {
            auto* cam = go->AddComponent<CameraComponent>();
            cam->SetFov(ExtractFloat(compBlock, kKeyFov, 0.45f));
            cam->SetNearZ(ExtractFloat(compBlock, kKeyNearZ, 0.1f));
            cam->SetFarZ(ExtractFloat(compBlock, kKeyFarZ, 1000.0f));
            cam->SetMainCamera(ExtractBool(compBlock, kKeyIsMainCamera, true));
            cam->SetUseTarget(ExtractBool(compBlock, kKeyUseTarget, false));
            cam->SetTarget(ExtractVector3(compBlock, kKeyTarget, { 0.0f, 0.0f, 0.0f }));
        } else if (type == kTypeLight) {
            auto* light = go->AddComponent<LightComponent>();
            std::string ltStr = ExtractString(compBlock, kKeyLightType, "Directional");
            if (ltStr == "Point") light->SetLightType(LightComponent::LightType::Point);
            else if (ltStr == "Spot") light->SetLightType(LightComponent::LightType::Spot);
            else light->SetLightType(LightComponent::LightType::Directional);

            light->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));
            light->SetIntensity(ExtractFloat(compBlock, kKeyIntensity, 1.0f));
            light->SetRange(ExtractFloat(compBlock, kKeyRange, 10.0f));
            light->SetDecay(ExtractFloat(compBlock, kKeyDecay, 1.0f));
            light->SetSpotAngle(ExtractFloat(compBlock, kKeySpotAngle, 45.0f));
            light->SetSpotFalloffStart(ExtractFloat(compBlock, kKeySpotFalloffStart, 30.0f));
        } else if (type == kTypeTextRenderer2D) {
            auto* tr = go->AddComponent<TextRenderer2DComponent>();
            tr->SetText(ExtractString(compBlock, kKeyText, "Text"));
            tr->SetFontName(ExtractString(compBlock, kKeyFontName, "Yu Gothic UI"));
            tr->SetFontSize(ExtractFloat(compBlock, kKeyFontSize, 32.0f));
            tr->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));
            tr->SetAlignment(ExtractInt(compBlock, kKeyAlignment, 0));
            tr->SetBold(ExtractBool(compBlock, kKeyIsBold, false));
            tr->SetItalic(ExtractBool(compBlock, kKeyIsItalic, false));
            tr->SetOutlineEnabled(ExtractBool(compBlock, kKeyEnableOutline, false));
            tr->SetOutlineColor(ExtractVector4(compBlock, kKeyOutlineColor, { 0.0f, 0.0f, 0.0f, 1.0f }));
            constexpr float kDefaultOutlineWidth = 2.0f;
            tr->SetOutlineWidth(ExtractFloat(compBlock, kKeyOutlineWidth, kDefaultOutlineWidth));

            Transform off{};
            off.translate = ExtractVector3(compBlock, kKeyOffsetPosition, { 0.0f, 0.0f, 0.0f });
            off.rotate = ExtractVector3(compBlock, kKeyOffsetRotation, { 0.0f, 0.0f, 0.0f });
            off.scale = ExtractVector3(compBlock, kKeyOffsetScale, { 1.0f, 1.0f, 1.0f });
            tr->SetOffset(off);
        } else if (type == kTypeTextRenderer3D) {
            auto* tr = go->AddComponent<TextRenderer3DComponent>();
            tr->SetText(ExtractString(compBlock, kKeyText, "3D Text"));
            tr->SetFontName(ExtractString(compBlock, kKeyFontName, "Yu Gothic UI"));
            tr->SetFontSize(ExtractFloat(compBlock, kKeyFontSize, 48.0f));
            tr->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));
            tr->SetAlignment(ExtractInt(compBlock, kKeyAlignment, 1));
            int bmVal = ExtractInt(compBlock, kKeyBillboardMode, static_cast<int>(TextObject3D::BillboardMode::AllAxis));
            tr->SetBillboardMode(static_cast<TextObject3D::BillboardMode>(bmVal));
            tr->SetWorldScale(ExtractFloat(compBlock, kKeyWorldScale, 0.02f));
            tr->SetLightingMode(ExtractInt(compBlock, kKeyLightingMode, 0));
            tr->SetBold(ExtractBool(compBlock, kKeyIsBold, false));
            tr->SetItalic(ExtractBool(compBlock, kKeyIsItalic, false));
            tr->SetOutlineEnabled(ExtractBool(compBlock, kKeyEnableOutline, false));
            tr->SetOutlineColor(ExtractVector4(compBlock, kKeyOutlineColor, { 0.0f, 0.0f, 0.0f, 1.0f }));
            constexpr float kDefaultOutlineWidth = 2.0f;
            tr->SetOutlineWidth(ExtractFloat(compBlock, kKeyOutlineWidth, kDefaultOutlineWidth));

            Transform off{};
            off.translate = ExtractVector3(compBlock, kKeyOffsetPosition, { 0.0f, 0.0f, 0.0f });
            off.rotate = ExtractVector3(compBlock, kKeyOffsetRotation, { 0.0f, 0.0f, 0.0f });
            off.scale = ExtractVector3(compBlock, kKeyOffsetScale, { 1.0f, 1.0f, 1.0f });
            tr->SetOffset(off);
        } else if (type == kTypeCameraOrbit) {
            auto* co = go->AddComponent<CameraOrbitComponent>();
            co->SetTargetCenter(ExtractVector3(compBlock, kKeyTargetCenter, { 0.0f, 0.0f, 0.0f }));
            co->SetRadius(ExtractFloat(compBlock, kKeyRadius, 25.0f));
            co->SetHeight(ExtractFloat(compBlock, kKeyHeight, 4.0f));
            co->SetOrbitSpeed(ExtractFloat(compBlock, kKeyOrbitSpeed, 0.15f));
        } else if (type == kTypeScriptComponent) {
            std::string scriptName = ExtractString(compBlock, kKeyScriptName, "ScriptComponent");
            std::string targetUUID = ExtractString(compBlock, kKeyTargetUUID, "");
            auto script = ScriptRegistry::GetInstance().CreateScript(scriptName);
            if (script) {
                script->SetTargetUUID(targetUUID);
                go->AddComponentInstance(std::move(script));
            }
        }
    }

    return go;
}
