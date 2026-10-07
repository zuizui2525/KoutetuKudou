#include "App/Scene/Core/SceneSerializer.h"
#include "Engine/Component/GameObject.h"
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

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>

namespace {
    // JSON構文・キー定数 (マジックナンバー・マジックストリング排除)
    constexpr const char* kKeySceneName = "scene_name";
    constexpr const char* kKeyGameObjects = "game_objects";
    constexpr const char* kKeyName = "name";
    constexpr const char* kKeyVisible = "visible";
    constexpr const char* kKeyTransform = "transform";
    constexpr const char* kKeyPosition = "position";
    constexpr const char* kKeyRotation = "rotation";
    constexpr const char* kKeyScale = "scale";
    constexpr const char* kKeyComponents = "components";
    constexpr const char* kKeyType = "type";

    // MeshRenderer キー
    constexpr const char* kTypeMeshRenderer = "MeshRenderer";
    constexpr const char* kKeyMeshType = "mesh_type";
    constexpr const char* kKeyTextureKey = "texture_key";
    constexpr const char* kKeyModelKey = "model_key";
    constexpr const char* kKeyEnvMapKey = "env_map_key";
    constexpr const char* kKeyColor = "color";
    constexpr const char* kKeyLightingMode = "lighting_mode";
    constexpr const char* kKeyShininess = "shininess";
    constexpr const char* kKeyEnvironmentCoefficient = "environment_coefficient";

    // SpriteRenderer キー
    constexpr const char* kTypeSpriteRenderer = "SpriteRenderer";
    constexpr const char* kKeyShapeType = "shape_type";
    constexpr const char* kKeySize = "size";
    constexpr const char* kKeyInnerRadius = "inner_radius";
    constexpr const char* kKeyLineStart = "line_start";
    constexpr const char* kKeyLineEnd = "line_end";
    constexpr const char* kKeyThickness = "thickness";

    // BoxCollider / SphereCollider キー
    constexpr const char* kTypeBoxCollider = "BoxCollider";
    constexpr const char* kTypeSphereCollider = "SphereCollider";
    constexpr const char* kKeyCenter = "center";
    constexpr const char* kKeyRadius = "radius";
    constexpr const char* kKeyIsTrigger = "is_trigger";
    constexpr const char* kKeyShowGizmo = "show_gizmo";

    // AudioSource キー
    constexpr const char* kTypeAudioSource = "AudioSource";
    constexpr const char* kKeyFilePath = "file_path";
    constexpr const char* kKeyVolume = "volume";
    constexpr const char* kKeyLoop = "loop";
    constexpr const char* kKeyAutoPlay = "auto_play";

    // ParticleEffect キー
    constexpr const char* kTypeParticleEffect = "ParticleEffect";
    constexpr const char* kKeyEffectName = "effect_name";
    constexpr const char* kKeyOffset = "offset";

    // Camera キー
    constexpr const char* kTypeCamera = "Camera";
    constexpr const char* kKeyFov = "fov";
    constexpr const char* kKeyNearZ = "near_z";
    constexpr const char* kKeyFarZ = "far_z";
    constexpr const char* kKeyIsMainCamera = "is_main_camera";
    constexpr const char* kKeyUseTarget = "use_target";
    constexpr const char* kKeyTarget = "target";

    // Light キー
    constexpr const char* kTypeLight = "Light";
    constexpr const char* kKeyLightType = "light_type";
    constexpr const char* kKeyIntensity = "intensity";
    constexpr const char* kKeyRange = "range";
    constexpr const char* kKeyDecay = "decay";
    constexpr const char* kKeySpotAngle = "spot_angle";
    constexpr const char* kKeySpotFalloffStart = "spot_falloff_start";

    // TextRenderer キー
    constexpr const char* kTypeTextRenderer2D = "TextRenderer2D";
    constexpr const char* kTypeTextRenderer3D = "TextRenderer3D";
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

    // Rotator キー
    constexpr const char* kTypeRotator = "Rotator";
    constexpr const char* kKeyRotationSpeed = "rotation_speed";

    // SinOscillator キー
    constexpr const char* kTypeSinOscillator = "SinOscillator";
    constexpr const char* kKeyTargetProperty = "target_property";
    constexpr const char* kKeyAmplitude = "amplitude";
    constexpr const char* kKeyFrequency = "frequency";
    constexpr const char* kKeyPhase = "phase";

    // Blink キー
    constexpr const char* kTypeBlink = "Blink";
    constexpr const char* kKeyBlinkSpeed = "blink_speed";
    constexpr const char* kKeyMinAlpha = "min_alpha";
    constexpr const char* kKeyMaxAlpha = "max_alpha";

    // CameraOrbit キー
    constexpr const char* kTypeCameraOrbit = "CameraOrbit";
    constexpr const char* kKeyTargetCenter = "target_center";
    constexpr const char* kKeyHeight = "height";
    constexpr const char* kKeyOrbitSpeed = "orbit_speed";

    // コンポーネント共通 Offset キー
    constexpr const char* kKeyOffsetPosition = "offset_position";
    constexpr const char* kKeyOffsetRotation = "offset_rotation";
    constexpr const char* kKeyOffsetScale = "offset_scale";

    // 文字列トリム補助関数
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    // JSONブロックから特定キーの文字列値を抽出
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

    // JSONブロックから浮動小数点数値を抽出
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

    // JSONブロックから整数値を抽出
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

    // JSONブロックからブール値を抽出
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

    // JSONブロックから数値配列（float）を抽出
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

    // JSONブロックからVector2を抽出
    Vector2 ExtractVector2(const std::string& block, const std::string& key, const Vector2& defaultVal = { 0.0f, 0.0f }) {
        auto vals = ParseFloatArray(block, key);
        if (vals.size() >= 2) {
            return { vals[0], vals[1] };
        }
        return defaultVal;
    }

    // JSONブロックからVector3を抽出
    Vector3 ExtractVector3(const std::string& block, const std::string& key, const Vector3& defaultVal = { 0.0f, 0.0f, 0.0f }) {
        auto vals = ParseFloatArray(block, key);
        if (vals.size() >= 3) {
            return { vals[0], vals[1], vals[2] };
        }
        return defaultVal;
    }

    // JSONブロックからVector4を抽出
    Vector4 ExtractVector4(const std::string& block, const std::string& key, const Vector4& defaultVal = { 1.0f, 1.0f, 1.0f, 1.0f }) {
        auto vals = ParseFloatArray(block, key);
        if (vals.size() >= 4) {
            return { vals[0], vals[1], vals[2], vals[3] };
        }
        return defaultVal;
    }

    // ネストされたオブジェクトブロック {...} を抽出する補助関数
    std::vector<std::string> ExtractObjectsFromArray(const std::string& json, const std::string& arrayKey) {
        std::vector<std::string> objects;
        std::string searchKey = "\"" + arrayKey + "\":";
        size_t arrStart = json.find(searchKey);
        if (arrStart == std::string::npos) return objects;

        size_t openBracket = json.find('[', arrStart);
        if (openBracket == std::string::npos) return objects;

        int depth = 0;
        size_t objStart = std::string::npos;
        for (size_t i = openBracket + 1; i < json.length(); ++i) {
            char c = json[i];
            if (c == '[') {
                // 配列内の入れ子配列（ベクトル等）
            } else if (c == ']' && depth == 0) {
                // 配列の終端
                break;
            } else if (c == '{') {
                if (depth == 0) {
                    objStart = i;
                }
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && objStart != std::string::npos) {
                    objects.push_back(json.substr(objStart, i - objStart + 1));
                    objStart = std::string::npos;
                }
            }
        }
        return objects;
    }
}

bool SceneSerializer::SaveScene(
    const std::string& filePath,
    const std::string& sceneName,
    const std::vector<std::unique_ptr<GameObject>>& gameObjects) {
    std::vector<GameObject*> rawList;
    rawList.reserve(gameObjects.size());
    for (const auto& go : gameObjects) {
        if (go) rawList.push_back(go.get());
    }
    return SaveScene(filePath, sceneName, rawList);
}

bool SceneSerializer::SaveScene(
    const std::string& filePath,
    const std::string& sceneName,
    const std::vector<GameObject*>& gameObjects) {

    // 保存先フォルダの作成
    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    ofs << "{\n";
    ofs << "  \"" << kKeySceneName << "\": \"" << sceneName << "\",\n";
    ofs << "  \"" << kKeyGameObjects << "\": [\n";

    for (size_t i = 0; i < gameObjects.size(); ++i) {
        GameObject* go = gameObjects[i];
        if (!go) continue;

        const auto& t = go->GetTransform();
        ofs << "    {\n";
        ofs << "      \"" << kKeyName << "\": \"" << go->GetName() << "\",\n";
        ofs << "      \"" << kKeyVisible << "\": " << (go->IsVisible() ? "true" : "false") << ",\n";
        ofs << "      \"" << kKeyTransform << "\": {\n";
        ofs << "        \"" << kKeyPosition << "\": [" << t.translate.x << ", " << t.translate.y << ", " << t.translate.z << "],\n";
        ofs << "        \"" << kKeyRotation << "\": [" << t.rotate.x << ", " << t.rotate.y << ", " << t.rotate.z << "],\n";
        ofs << "        \"" << kKeyScale << "\": [" << t.scale.x << ", " << t.scale.y << ", " << t.scale.z << "]\n";
        ofs << "      },\n";

        // コンポーネント群のシリアライズ
        ofs << "      \"" << kKeyComponents << "\": [\n";
        const auto& components = go->GetComponents();
        for (size_t c = 0; c < components.size(); ++c) {
            auto& comp = components[c];
            if (!comp) continue;

            std::string typeName = comp->GetComponentTypeName();
            ofs << "        {\n";
            ofs << "          \"" << kKeyType << "\": \"" << typeName << "\"";

            if (typeName == kTypeMeshRenderer) {
                if (auto* mr = dynamic_cast<MeshRendererComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyMeshType << "\": " << static_cast<int>(mr->GetMeshType()) << ",\n";
                    ofs << "          \"" << kKeyTextureKey << "\": \"" << mr->GetTextureKey() << "\",\n";
                    ofs << "          \"" << kKeyModelKey << "\": \"" << mr->GetModelKey() << "\",\n";
                    ofs << "          \"" << kKeyEnvMapKey << "\": \"" << mr->GetEnvMapKey() << "\",\n";
                    const auto& col = mr->GetColor();
                    ofs << "          \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"" << kKeyLightingMode << "\": " << mr->GetLightingMode() << ",\n";
                    ofs << "          \"" << kKeyShininess << "\": " << mr->GetShininess() << ",\n";
                    ofs << "          \"" << kKeyEnvironmentCoefficient << "\": " << mr->GetEnvironmentCoefficient() << ",\n";
                    const auto& off = mr->GetOffset();
                    ofs << "          \"" << kKeyOffsetPosition << "\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetRotation << "\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetScale << "\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == kTypeSpriteRenderer) {
                if (auto* sr = dynamic_cast<SpriteRendererComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyShapeType << "\": " << static_cast<int>(sr->GetShapeType()) << ",\n";
                    ofs << "          \"" << kKeyTextureKey << "\": \"" << sr->GetTextureKey() << "\",\n";
                    const auto& sz = sr->GetSize();
                    ofs << "          \"" << kKeySize << "\": [" << sz.x << ", " << sz.y << "],\n";
                    ofs << "          \"" << kKeyRadius << "\": " << sr->GetRadius() << ",\n";
                    ofs << "          \"" << kKeyInnerRadius << "\": " << sr->GetInnerRadius() << ",\n";
                    const auto& ls = sr->GetLineStart();
                    ofs << "          \"" << kKeyLineStart << "\": [" << ls.x << ", " << ls.y << "],\n";
                    const auto& le = sr->GetLineEnd();
                    ofs << "          \"" << kKeyLineEnd << "\": [" << le.x << ", " << le.y << "],\n";
                    ofs << "          \"" << kKeyThickness << "\": " << sr->GetLineThickness() << ",\n";
                    const auto& col = sr->GetColor();
                    ofs << "          \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    const auto& off = sr->GetOffset();
                    ofs << "          \"" << kKeyOffsetPosition << "\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetRotation << "\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetScale << "\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == kTypeBoxCollider) {
                if (auto* bc = dynamic_cast<BoxColliderComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& center = bc->GetCenterOffset();
                    const auto& size = bc->GetSize();
                    ofs << "          \"" << kKeyCenter << "\": [" << center.x << ", " << center.y << ", " << center.z << "],\n";
                    ofs << "          \"" << kKeySize << "\": [" << size.x << ", " << size.y << ", " << size.z << "],\n";
                    ofs << "          \"" << kKeyIsTrigger << "\": " << (bc->IsTrigger() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyShowGizmo << "\": " << (bc->IsShowDebugGizmo() ? "true" : "false");
                }
            } else if (typeName == kTypeSphereCollider) {
                if (auto* sc = dynamic_cast<SphereColliderComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& center = sc->GetCenterOffset();
                    ofs << "          \"" << kKeyCenter << "\": [" << center.x << ", " << center.y << ", " << center.z << "],\n";
                    ofs << "          \"" << kKeyRadius << "\": " << sc->GetRadius() << ",\n";
                    ofs << "          \"" << kKeyIsTrigger << "\": " << (sc->IsTrigger() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyShowGizmo << "\": " << (sc->IsShowDebugGizmo() ? "true" : "false");
                }
            } else if (typeName == kTypeAudioSource) {
                if (auto* as = dynamic_cast<AudioSourceComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyFilePath << "\": \"" << as->GetFilePath() << "\",\n";
                    ofs << "          \"" << kKeyVolume << "\": " << as->GetVolume() << ",\n";
                    ofs << "          \"" << kKeyLoop << "\": " << (as->IsLoop() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyAutoPlay << "\": " << (as->IsAutoPlay() ? "true" : "false");
                }
            } else if (typeName == kTypeParticleEffect) {
                if (auto* pe = dynamic_cast<ParticleEffectComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyEffectName << "\": \"" << pe->GetEffectName() << "\",\n";
                    const auto& off = pe->GetOffset();
                    ofs << "          \"" << kKeyOffset << "\": [" << off.x << ", " << off.y << ", " << off.z << "],\n";
                    ofs << "          \"" << kKeyLoop << "\": " << (pe->IsLoop() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyAutoPlay << "\": " << (pe->IsAutoPlay() ? "true" : "false");
                }
            } else if (typeName == kTypeCamera) {
                if (auto* cam = dynamic_cast<CameraComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyFov << "\": " << cam->GetFov() << ",\n";
                    ofs << "          \"" << kKeyNearZ << "\": " << cam->GetNearZ() << ",\n";
                    ofs << "          \"" << kKeyFarZ << "\": " << cam->GetFarZ() << ",\n";
                    ofs << "          \"" << kKeyIsMainCamera << "\": " << (cam->IsMainCamera() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyUseTarget << "\": " << (cam->IsUseTarget() ? "true" : "false") << ",\n";
                    const auto& tgt = cam->GetTarget();
                    ofs << "          \"" << kKeyTarget << "\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "]";
                }
            } else if (typeName == kTypeLight) {
                if (auto* light = dynamic_cast<LightComponent*>(comp.get())) {
                    ofs << ",\n";
                    std::string lightTypeStr = "Directional";
                    if (light->GetLightType() == LightComponent::LightType::Point) lightTypeStr = "Point";
                    else if (light->GetLightType() == LightComponent::LightType::Spot) lightTypeStr = "Spot";

                    ofs << "          \"" << kKeyLightType << "\": \"" << lightTypeStr << "\",\n";
                    const auto& col = light->GetColor();
                    ofs << "          \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"" << kKeyIntensity << "\": " << light->GetIntensity() << ",\n";
                    ofs << "          \"" << kKeyRange << "\": " << light->GetRange() << ",\n";
                    ofs << "          \"" << kKeyDecay << "\": " << light->GetDecay() << ",\n";
                    ofs << "          \"" << kKeySpotAngle << "\": " << light->GetSpotAngle() << ",\n";
                    ofs << "          \"" << kKeySpotFalloffStart << "\": " << light->GetSpotFalloffStart();
                }
            } else if (typeName == kTypeTextRenderer2D) {
                if (auto* tr = dynamic_cast<TextRenderer2DComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyText << "\": \"" << tr->GetText() << "\",\n";
                    ofs << "          \"" << kKeyFontName << "\": \"" << tr->GetFontName() << "\",\n";
                    ofs << "          \"" << kKeyFontSize << "\": " << tr->GetFontSize() << ",\n";
                    const auto& col = tr->GetColor();
                    ofs << "          \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"" << kKeyAlignment << "\": " << tr->GetAlignment() << ",\n";
                    ofs << "          \"" << kKeyIsBold << "\": " << (tr->IsBold() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyIsItalic << "\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyEnableOutline << "\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
                    const auto& outCol = tr->GetOutlineColor();
                    ofs << "          \"" << kKeyOutlineColor << "\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
                    ofs << "          \"" << kKeyOutlineWidth << "\": " << tr->GetOutlineWidth() << ",\n";
                    const auto& off = tr->GetOffset();
                    ofs << "          \"" << kKeyOffsetPosition << "\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetRotation << "\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetScale << "\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == kTypeTextRenderer3D) {
                if (auto* tr = dynamic_cast<TextRenderer3DComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyText << "\": \"" << tr->GetText() << "\",\n";
                    ofs << "          \"" << kKeyFontName << "\": \"" << tr->GetFontName() << "\",\n";
                    ofs << "          \"" << kKeyFontSize << "\": " << tr->GetFontSize() << ",\n";
                    const auto& col = tr->GetColor();
                    ofs << "          \"" << kKeyColor << "\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"" << kKeyAlignment << "\": " << tr->GetAlignment() << ",\n";
                    ofs << "          \"" << kKeyBillboardMode << "\": " << static_cast<int>(tr->GetBillboardMode()) << ",\n";
                    ofs << "          \"" << kKeyWorldScale << "\": " << tr->GetWorldScale() << ",\n";
                    ofs << "          \"" << kKeyLightingMode << "\": " << tr->GetLightingMode() << ",\n";
                    ofs << "          \"" << kKeyIsBold << "\": " << (tr->IsBold() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyIsItalic << "\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
                    ofs << "          \"" << kKeyEnableOutline << "\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
                    const auto& outCol = tr->GetOutlineColor();
                    ofs << "          \"" << kKeyOutlineColor << "\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
                    ofs << "          \"" << kKeyOutlineWidth << "\": " << tr->GetOutlineWidth() << ",\n";
                    const auto& off = tr->GetOffset();
                    ofs << "          \"" << kKeyOffsetPosition << "\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetRotation << "\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"" << kKeyOffsetScale << "\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == kTypeRotator) {
                if (auto* rot = dynamic_cast<RotatorComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& spd = rot->GetRotationSpeed();
                    ofs << "          \"" << kKeyRotationSpeed << "\": [" << spd.x << ", " << spd.y << ", " << spd.z << "]";
                }
            } else if (typeName == kTypeSinOscillator) {
                if (auto* osc = dynamic_cast<SinOscillatorComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyTargetProperty << "\": " << static_cast<int>(osc->GetTargetType()) << ",\n";
                    const auto& amp = osc->GetAmplitude();
                    ofs << "          \"" << kKeyAmplitude << "\": [" << amp.x << ", " << amp.y << ", " << amp.z << "],\n";
                    ofs << "          \"" << kKeyFrequency << "\": " << osc->GetFrequency() << ",\n";
                    ofs << "          \"" << kKeyPhase << "\": " << osc->GetPhase();
                }
            } else if (typeName == kTypeBlink) {
                if (auto* blk = dynamic_cast<BlinkComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"" << kKeyBlinkSpeed << "\": " << blk->GetBlinkSpeed() << ",\n";
                    ofs << "          \"" << kKeyMinAlpha << "\": " << blk->GetMinAlpha() << ",\n";
                    ofs << "          \"" << kKeyMaxAlpha << "\": " << blk->GetMaxAlpha();
                }
            } else if (typeName == kTypeCameraOrbit) {
                if (auto* co = dynamic_cast<CameraOrbitComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& tgt = co->GetTargetCenter();
                    ofs << "          \"" << kKeyTargetCenter << "\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "],\n";
                    ofs << "          \"" << kKeyRadius << "\": " << co->GetRadius() << ",\n";
                    ofs << "          \"" << kKeyHeight << "\": " << co->GetHeight() << ",\n";
                    ofs << "          \"" << kKeyOrbitSpeed << "\": " << co->GetOrbitSpeed();
                }
            }

            ofs << "\n        }";
            if (c + 1 < components.size()) ofs << ",";
            ofs << "\n";
        }

        ofs << "      ]\n";
        ofs << "    }";
        if (i + 1 < gameObjects.size()) ofs << ",";
        ofs << "\n";
    }

    ofs << "  ]\n";
    ofs << "}\n";

    return true;
}

bool SceneSerializer::LoadScene(
    const std::string& filePath,
    std::string& outSceneName,
    std::vector<std::unique_ptr<GameObject>>& outGameObjects) {

    if (!std::filesystem::exists(filePath)) return false;

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string json = ss.str();

    // 1. シーン名の抽出
    outSceneName = ExtractString(json, kKeySceneName, "Untitled");

    // 2. GameObjectブロックの抽出
    auto goBlocks = ExtractObjectsFromArray(json, kKeyGameObjects);
    outGameObjects.clear();
    outGameObjects.reserve(goBlocks.size());

    for (const auto& goBlock : goBlocks) {
        std::string name = ExtractString(goBlock, kKeyName, "GameObject");
        bool visible = ExtractBool(goBlock, kKeyVisible, true);

        auto go = std::make_unique<GameObject>(name);
        go->SetVisible(visible);

        // Transform の復元
        Vector3 pos = ExtractVector3(goBlock, kKeyPosition, { 0.0f, 0.0f, 0.0f });
        Vector3 rot = ExtractVector3(goBlock, kKeyRotation, { 0.0f, 0.0f, 0.0f });
        Vector3 scale = ExtractVector3(goBlock, kKeyScale, { 1.0f, 1.0f, 1.0f });

        go->SetPosition(pos);
        go->SetRotate(rot);
        go->SetScale(scale);
        go->UpdateMatrix();

        // 3. コンポーネント群の抽出と復元
        auto compBlocks = ExtractObjectsFromArray(goBlock, kKeyComponents);
        for (const auto& compBlock : compBlocks) {
            std::string type = ExtractString(compBlock, kKeyType);

            if (type == kTypeMeshRenderer) {
                auto* mr = go->AddComponent<MeshRendererComponent>();
                int meshType = ExtractInt(compBlock, kKeyMeshType, 0);
                mr->SetMeshType(static_cast<MeshRendererComponent::MeshType>(meshType));
                mr->SetTextureKey(ExtractString(compBlock, kKeyTextureKey, "white"));
                mr->SetModelKey(ExtractString(compBlock, kKeyModelKey, "cube"));
                mr->SetEnvMapKey(ExtractString(compBlock, kKeyEnvMapKey, ""));
                mr->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));
                mr->SetLightingMode(ExtractInt(compBlock, kKeyLightingMode, 1));
                mr->SetShininess(ExtractFloat(compBlock, kKeyShininess, 30.0f));
                mr->SetEnvironmentCoefficient(ExtractFloat(compBlock, kKeyEnvironmentCoefficient, 1.0f));

                Transform off{};
                off.translate = ExtractVector3(compBlock, kKeyOffsetPosition, { 0.0f, 0.0f, 0.0f });
                off.rotate = ExtractVector3(compBlock, kKeyOffsetRotation, { 0.0f, 0.0f, 0.0f });
                off.scale = ExtractVector3(compBlock, kKeyOffsetScale, { 1.0f, 1.0f, 1.0f });
                mr->SetOffset(off);
            } else if (type == kTypeSpriteRenderer) {
                auto* sr = go->AddComponent<SpriteRendererComponent>();
                int shapeType = ExtractInt(compBlock, kKeyShapeType, 0);
                sr->SetShapeType(static_cast<SpriteRendererComponent::ShapeType>(shapeType));
                sr->SetTextureKey(ExtractString(compBlock, kKeyTextureKey, "white"));
                sr->SetSize(ExtractVector2(compBlock, kKeySize, { 100.0f, 100.0f }));
                sr->SetRadius(ExtractFloat(compBlock, kKeyRadius, 50.0f));
                sr->SetInnerRadius(ExtractFloat(compBlock, kKeyInnerRadius, 35.0f));
                sr->SetLineStart(ExtractVector2(compBlock, kKeyLineStart, { 0.0f, 0.0f }));
                sr->SetLineEnd(ExtractVector2(compBlock, kKeyLineEnd, { 150.0f, 0.0f }));
                sr->SetLineThickness(ExtractFloat(compBlock, kKeyThickness, 4.0f));
                sr->SetColor(ExtractVector4(compBlock, kKeyColor, { 1.0f, 1.0f, 1.0f, 1.0f }));

                Transform off{};
                off.translate = ExtractVector3(compBlock, kKeyOffsetPosition, { 0.0f, 0.0f, 0.0f });
                off.rotate = ExtractVector3(compBlock, kKeyOffsetRotation, { 0.0f, 0.0f, 0.0f });
                off.scale = ExtractVector3(compBlock, kKeyOffsetScale, { 1.0f, 1.0f, 1.0f });
                sr->SetOffset(off);
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
            } else if (type == kTypeAudioSource) {
                auto* as = go->AddComponent<AudioSourceComponent>();
                std::string soundPath = ExtractString(compBlock, kKeyFilePath, "");
                as->SetFilePath(soundPath);
                as->SetVolume(ExtractFloat(compBlock, kKeyVolume, 1.0f));
                as->SetLoop(ExtractBool(compBlock, kKeyLoop, false));
                as->SetAutoPlay(ExtractBool(compBlock, kKeyAutoPlay, false));
                if (!soundPath.empty()) {
                    as->Load(soundPath);
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
            } else if (type == kTypeCameraOrbit) {
                auto* co = go->AddComponent<CameraOrbitComponent>();
                co->SetTargetCenter(ExtractVector3(compBlock, kKeyTargetCenter, { 0.0f, 0.0f, 0.0f }));
                co->SetRadius(ExtractFloat(compBlock, kKeyRadius, 25.0f));
                co->SetHeight(ExtractFloat(compBlock, kKeyHeight, 4.0f));
                co->SetOrbitSpeed(ExtractFloat(compBlock, kKeyOrbitSpeed, 0.15f));
            }
        }

        outGameObjects.push_back(std::move(go));
    }

    return true;
}

bool SceneSerializer::RunSelfTest() {
    // 1. テスト用GameObject群の構築
    std::vector<std::unique_ptr<GameObject>> testObjects;
    auto go = std::make_unique<GameObject>("TestEntity");
    go->SetPosition({ 1.0f, 2.0f, 3.0f });
    go->SetRotate({ 0.1f, 0.2f, 0.3f });
    go->SetScale({ 2.0f, 2.0f, 2.0f });

    // コンポーネント群をアタッチ
    auto* mr = go->AddComponent<MeshRendererComponent>();
    mr->SetMeshType(MeshRendererComponent::MeshType::Cube);
    mr->SetColor({ 1.0f, 0.5f, 0.2f, 1.0f });

    auto* bc = go->AddComponent<BoxColliderComponent>();
    bc->SetSize({ 2.0f, 2.0f, 2.0f });

    auto* as = go->AddComponent<AudioSourceComponent>();
    as->SetFilePath("resources/sounds/test.wav");
    as->SetVolume(0.75f);

    auto* pe = go->AddComponent<ParticleEffectComponent>();
    pe->SetEffectName("Hit");

    auto* tr2 = go->AddComponent<TextRenderer2DComponent>();
    tr2->SetText("SelfTest2D");

    auto* tr3 = go->AddComponent<TextRenderer3DComponent>();
    tr3->SetText("SelfTest3D");
    tr3->SetBillboardMode(TextObject3D::BillboardMode::YAxisOnly);

    testObjects.push_back(std::move(go));

    // 2. 保存テスト
    const std::string testPath = "resources/Scenes/test_scene.json";
    if (!SaveScene(testPath, "TestScene", testObjects)) {
        return false;
    }

    // 元のテストオブジェクトを解放し、SceneHierarchyの名前重複（_1付加）を防止
    testObjects.clear();

    // 3. 読込テスト
    std::string loadedSceneName;
    std::vector<std::unique_ptr<GameObject>> loadedObjects;
    if (!LoadScene(testPath, loadedSceneName, loadedObjects)) {
        return false;
    }

    // 4. アサーション検証
    if (loadedSceneName != "TestScene") return false;
    if (loadedObjects.size() != 1) return false;
    if (loadedObjects[0]->GetName() != "TestEntity") return false;
    if (std::abs(loadedObjects[0]->GetPosition().x - 1.0f) > 0.001f) return false;
    if (std::abs(loadedObjects[0]->GetPosition().y - 2.0f) > 0.001f) return false;
    if (std::abs(loadedObjects[0]->GetPosition().z - 3.0f) > 0.001f) return false;
    if (loadedObjects[0]->GetComponents().size() != 6) return false;
    if (!loadedObjects[0]->HasComponent<MeshRendererComponent>()) return false;
    if (!loadedObjects[0]->HasComponent<BoxColliderComponent>()) return false;
    if (!loadedObjects[0]->HasComponent<AudioSourceComponent>()) return false;
    if (!loadedObjects[0]->HasComponent<ParticleEffectComponent>()) return false;
    if (!loadedObjects[0]->HasComponent<TextRenderer2DComponent>()) return false;
    if (!loadedObjects[0]->HasComponent<TextRenderer3DComponent>()) return false;

    auto* loadedTr3 = loadedObjects[0]->GetComponent<TextRenderer3DComponent>();
    if (loadedTr3->GetText() != "SelfTest3D") return false;
    if (loadedTr3->GetBillboardMode() != TextObject3D::BillboardMode::YAxisOnly) return false;

    return true;
}
