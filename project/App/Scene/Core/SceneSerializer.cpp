#include "App/Scene/Core/SceneSerializer.h"
#include "Engine/Component/GameObject.h"
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
#include "Scripts/Stage/SkydomeScript.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <unordered_map>

namespace {
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

std::string SceneSerializer::SerializeToString(
    const std::string& sceneName,
    const std::vector<std::unique_ptr<GameObject>>& gameObjects) {
    std::vector<GameObject*> rawList;
    rawList.reserve(gameObjects.size());
    for (const auto& go : gameObjects) {
        if (go) rawList.push_back(go.get());
    }
    return SerializeToString(sceneName, rawList);
}

std::string SceneSerializer::SerializeToString(
    const std::string& sceneName,
    const std::vector<GameObject*>& gameObjects) {

    std::ostringstream ofs;

    ofs << "{\n";
    ofs << "  \"scene_name\": \"" << sceneName << "\",\n";
    ofs << "  \"game_objects\": [\n";

    for (size_t i = 0; i < gameObjects.size(); ++i) {
        GameObject* go = gameObjects[i];
        if (!go) continue;

        const auto& t = go->GetTransform();
        ofs << "    {\n";
        ofs << "      \"uuid\": \"" << go->GetUUID() << "\",\n";
        ofs << "      \"name\": \"" << go->GetName() << "\",\n";
        ofs << "      \"visible\": " << (go->IsVisible() ? "true" : "false") << ",\n";
        ofs << "      \"transform\": {\n";
        ofs << "        \"position\": [" << t.translate.x << ", " << t.translate.y << ", " << t.translate.z << "],\n";
        ofs << "        \"rotation\": [" << t.rotate.x << ", " << t.rotate.y << ", " << t.rotate.z << "],\n";
        ofs << "        \"scale\": [" << t.scale.x << ", " << t.scale.y << ", " << t.scale.z << "]\n";
        ofs << "      },\n";

        // コンポーネント群のシリアライズ
        ofs << "      \"components\": [\n";
        const auto& components = go->GetComponents();
        for (size_t c = 0; c < components.size(); ++c) {
            auto& comp = components[c];
            if (!comp) continue;

            std::string typeName = comp->GetComponentTypeName();
            ofs << "        {\n";
            ofs << "          \"type\": \"" << typeName << "\"";

            if (typeName == "MeshRenderer") {
                if (auto* mr = dynamic_cast<MeshRendererComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"mesh_type\": " << static_cast<int>(mr->GetMeshType()) << ",\n";
                    ofs << "          \"texture_key\": \"" << mr->GetTextureKey() << "\",\n";
                    ofs << "          \"model_key\": \"" << mr->GetModelKey() << "\",\n";
                    ofs << "          \"animation_key\": \"" << mr->GetAnimationKey() << "\",\n";
                    ofs << "          \"is_animation_playing\": " << (mr->IsAnimationPlaying() ? "true" : "false") << ",\n";
                    ofs << "          \"show_skeleton\": " << (mr->IsShowSkeleton() ? "true" : "false") << ",\n";
                    ofs << "          \"env_map_key\": \"" << mr->GetEnvMapKey() << "\",\n";
                    const auto& col = mr->GetColor();
                    ofs << "          \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"lighting_mode\": " << mr->GetLightingMode() << ",\n";
                    ofs << "          \"shininess\": " << mr->GetShininess() << ",\n";
                    ofs << "          \"environment_coefficient\": " << mr->GetEnvironmentCoefficient() << ",\n";
                    const auto& off = mr->GetOffset();
                    ofs << "          \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == "SpriteRenderer") {
                if (auto* sr = dynamic_cast<SpriteRendererComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"shape_type\": " << static_cast<int>(sr->GetShapeType()) << ",\n";
                    ofs << "          \"texture_key\": \"" << sr->GetTextureKey() << "\",\n";
                    const auto& sz = sr->GetSize();
                    ofs << "          \"size\": [" << sz.x << ", " << sz.y << "],\n";
                    ofs << "          \"radius\": " << sr->GetRadius() << ",\n";
                    ofs << "          \"inner_radius\": " << sr->GetInnerRadius() << ",\n";
                    const auto& ls = sr->GetLineStart();
                    ofs << "          \"line_start\": [" << ls.x << ", " << ls.y << "],\n";
                    const auto& le = sr->GetLineEnd();
                    ofs << "          \"line_end\": [" << le.x << ", " << le.y << "],\n";
                    ofs << "          \"thickness\": " << sr->GetLineThickness() << ",\n";
                    const auto& col = sr->GetColor();
                    ofs << "          \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    const auto& off = sr->GetOffset();
                    ofs << "          \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == "BoxCollider") {
                if (auto* bc = dynamic_cast<BoxColliderComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& center = bc->GetCenterOffset();
                    const auto& size = bc->GetSize();
                    ofs << "          \"center\": [" << center.x << ", " << center.y << ", " << center.z << "],\n";
                    ofs << "          \"size\": [" << size.x << ", " << size.y << ", " << size.z << "],\n";
                    ofs << "          \"is_trigger\": " << (bc->IsTrigger() ? "true" : "false") << ",\n";
                    ofs << "          \"show_gizmo\": " << (bc->IsShowDebugGizmo() ? "true" : "false");
                }
            } else if (typeName == "SphereCollider") {
                if (auto* sc = dynamic_cast<SphereColliderComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& center = sc->GetCenterOffset();
                    ofs << "          \"center\": [" << center.x << ", " << center.y << ", " << center.z << "],\n";
                    ofs << "          \"radius\": " << sc->GetRadius() << ",\n";
                    ofs << "          \"is_trigger\": " << (sc->IsTrigger() ? "true" : "false") << ",\n";
                    ofs << "          \"show_gizmo\": " << (sc->IsShowDebugGizmo() ? "true" : "false");
                }
            } else if (typeName == "AudioSource") {
                if (auto* as = dynamic_cast<AudioSourceComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"file_path\": \"" << as->GetFilePath() << "\",\n";
                    ofs << "          \"volume\": " << as->GetVolume() << ",\n";
                    ofs << "          \"loop\": " << (as->IsLoop() ? "true" : "false") << ",\n";
                    ofs << "          \"auto_play\": " << (as->IsAutoPlay() ? "true" : "false");
                }
            } else if (typeName == "ParticleEffect") {
                if (auto* pe = dynamic_cast<ParticleEffectComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"effect_name\": \"" << pe->GetEffectName() << "\",\n";
                    const auto& off = pe->GetOffset();
                    ofs << "          \"offset\": [" << off.x << ", " << off.y << ", " << off.z << "],\n";
                    ofs << "          \"loop\": " << (pe->IsLoop() ? "true" : "false") << ",\n";
                    ofs << "          \"auto_play\": " << (pe->IsAutoPlay() ? "true" : "false");
                }
            } else if (typeName == "GPUParticle") {
                if (auto* gpuComp = dynamic_cast<GPUParticleComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"preset_name\": \"" << gpuComp->GetPresetName() << "\",\n";
                    ofs << "          \"texture_key\": \"" << gpuComp->GetTextureKey() << "\",\n";
                    const auto& off = gpuComp->GetOffset();
                    ofs << "          \"offset\": [" << off.x << ", " << off.y << ", " << off.z << "],\n";
                    ofs << "          \"auto_spawn\": " << (gpuComp->IsAutoSpawning() ? "true" : "false") << ",\n";
                    ofs << "          \"blend_mode\": " << static_cast<int>(gpuComp->GetBlendMode()) << ",\n";
                    ofs << "          \"is_loop\": " << (gpuComp->IsLoop() ? "true" : "false") << ",\n";
                    ofs << "          \"spawn_rate\": " << gpuComp->GetSpawnRate() << ",\n";
                    ofs << "          \"burst_interval\": " << gpuComp->GetBurstInterval() << ",\n";
                    ofs << "          \"burst_count\": " << gpuComp->GetBurstCount() << ",\n";
                    ofs << "          \"min_lifetime\": " << gpuComp->GetMinLifeTime() << ",\n";
                    ofs << "          \"max_lifetime\": " << gpuComp->GetMaxLifeTime() << ",\n";
                    ofs << "          \"gravity\": " << gpuComp->GetGravity() << ",\n";
                    const auto& sc = gpuComp->GetStartColor();
                    ofs << "          \"start_color\": [" << sc.x << ", " << sc.y << ", " << sc.z << ", " << sc.w << "],\n";
                    const auto& ec = gpuComp->GetEndColor();
                    ofs << "          \"end_color\": [" << ec.x << ", " << ec.y << ", " << ec.z << ", " << ec.w << "],\n";
                    ofs << "          \"firework_text\": \"" << gpuComp->GetFireworkText() << "\",\n";
                    ofs << "          \"launch_height\": " << gpuComp->GetLaunchHeight() << ",\n";
                    ofs << "          \"text_scale\": " << gpuComp->GetTextScale();
                }
            } else if (typeName == "Camera") {
                if (auto* cam = dynamic_cast<CameraComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"fov\": " << cam->GetFov() << ",\n";
                    ofs << "          \"near_z\": " << cam->GetNearZ() << ",\n";
                    ofs << "          \"far_z\": " << cam->GetFarZ() << ",\n";
                    ofs << "          \"is_main_camera\": " << (cam->IsMainCamera() ? "true" : "false") << ",\n";
                    ofs << "          \"use_target\": " << (cam->IsUseTarget() ? "true" : "false") << ",\n";
                    const auto& tgt = cam->GetTarget();
                    ofs << "          \"target\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "]";
                }
            } else if (typeName == "Light") {
                if (auto* light = dynamic_cast<LightComponent*>(comp.get())) {
                    ofs << ",\n";
                    std::string lightTypeStr = "Directional";
                    if (light->GetLightType() == LightComponent::LightType::Point) lightTypeStr = "Point";
                    else if (light->GetLightType() == LightComponent::LightType::Spot) lightTypeStr = "Spot";

                    ofs << "          \"light_type\": \"" << lightTypeStr << "\",\n";
                    const auto& col = light->GetColor();
                    ofs << "          \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"intensity\": " << light->GetIntensity() << ",\n";
                    ofs << "          \"range\": " << light->GetRange() << ",\n";
                    ofs << "          \"decay\": " << light->GetDecay() << ",\n";
                    ofs << "          \"spot_angle\": " << light->GetSpotAngle() << ",\n";
                    ofs << "          \"spot_falloff_start\": " << light->GetSpotFalloffStart();
                }
            } else if (typeName == "TextRenderer2D") {
                if (auto* tr = dynamic_cast<TextRenderer2DComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"text\": \"" << tr->GetText() << "\",\n";
                    ofs << "          \"font_name\": \"" << tr->GetFontName() << "\",\n";
                    ofs << "          \"font_size\": " << tr->GetFontSize() << ",\n";
                    const auto& col = tr->GetColor();
                    ofs << "          \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"alignment\": " << tr->GetAlignment() << ",\n";
                    ofs << "          \"is_bold\": " << (tr->IsBold() ? "true" : "false") << ",\n";
                    ofs << "          \"is_italic\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
                    ofs << "          \"enable_outline\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
                    const auto& outCol = tr->GetOutlineColor();
                    ofs << "          \"outline_color\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
                    ofs << "          \"outline_width\": " << tr->GetOutlineWidth() << ",\n";
                    const auto& off = tr->GetOffset();
                    ofs << "          \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == "TextRenderer3D") {
                if (auto* tr = dynamic_cast<TextRenderer3DComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"text\": \"" << tr->GetText() << "\",\n";
                    ofs << "          \"font_name\": \"" << tr->GetFontName() << "\",\n";
                    ofs << "          \"font_size\": " << tr->GetFontSize() << ",\n";
                    const auto& col = tr->GetColor();
                    ofs << "          \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                    ofs << "          \"alignment\": " << tr->GetAlignment() << ",\n";
                    ofs << "          \"billboard_mode\": " << static_cast<int>(tr->GetBillboardMode()) << ",\n";
                    ofs << "          \"world_scale\": " << tr->GetWorldScale() << ",\n";
                    ofs << "          \"lighting_mode\": " << tr->GetLightingMode() << ",\n";
                    ofs << "          \"is_bold\": " << (tr->IsBold() ? "true" : "false") << ",\n";
                    ofs << "          \"is_italic\": " << (tr->IsItalic() ? "true" : "false") << ",\n";
                    ofs << "          \"enable_outline\": " << (tr->IsOutlineEnabled() ? "true" : "false") << ",\n";
                    const auto& outCol = tr->GetOutlineColor();
                    ofs << "          \"outline_color\": [" << outCol.x << ", " << outCol.y << ", " << outCol.z << ", " << outCol.w << "],\n";
                    ofs << "          \"outline_width\": " << tr->GetOutlineWidth() << ",\n";
                    const auto& off = tr->GetOffset();
                    ofs << "          \"offset_position\": [" << off.translate.x << ", " << off.translate.y << ", " << off.translate.z << "],\n";
                    ofs << "          \"offset_rotation\": [" << off.rotate.x << ", " << off.rotate.y << ", " << off.rotate.z << "],\n";
                    ofs << "          \"offset_scale\": [" << off.scale.x << ", " << off.scale.y << ", " << off.scale.z << "]";
                }
            } else if (typeName == "Rotator") {
                if (auto* rot = dynamic_cast<RotatorComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& spd = rot->GetRotationSpeed();
                    ofs << "          \"rotation_speed\": [" << spd.x << ", " << spd.y << ", " << spd.z << "]";
                }
            } else if (typeName == "SinOscillator") {
                if (auto* osc = dynamic_cast<SinOscillatorComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"target_property\": " << static_cast<int>(osc->GetTargetType()) << ",\n";
                    const auto& amp = osc->GetAmplitude();
                    ofs << "          \"amplitude\": [" << amp.x << ", " << amp.y << ", " << amp.z << "],\n";
                    ofs << "          \"frequency\": " << osc->GetFrequency() << ",\n";
                    ofs << "          \"phase\": " << osc->GetPhase();
                }
            } else if (typeName == "Blink") {
                if (auto* blk = dynamic_cast<BlinkComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"blink_speed\": " << blk->GetBlinkSpeed() << ",\n";
                    ofs << "          \"min_alpha\": " << blk->GetMinAlpha() << ",\n";
                    ofs << "          \"max_alpha\": " << blk->GetMaxAlpha();
                }
            } else if (typeName == "CameraOrbit") {
                if (auto* co = dynamic_cast<CameraOrbitComponent*>(comp.get())) {
                    ofs << ",\n";
                    const auto& tgt = co->GetTargetCenter();
                    ofs << "          \"target_center\": [" << tgt.x << ", " << tgt.y << ", " << tgt.z << "],\n";
                    ofs << "          \"radius\": " << co->GetRadius() << ",\n";
                    ofs << "          \"height\": " << co->GetHeight() << ",\n";
                    ofs << "          \"orbit_speed\": " << co->GetOrbitSpeed();
                }
            } else if (typeName == "ScriptComponent") {
                if (auto* sc = dynamic_cast<ScriptComponent*>(comp.get())) {
                    ofs << ",\n";
                    ofs << "          \"script_name\": \"" << sc->GetScriptName() << "\",\n";
                    ofs << "          \"target_uuid\": \"" << sc->GetTargetUUID() << "\"";
                    if (auto* pc = dynamic_cast<PlayerControllerScript*>(sc)) {
                        ofs << ",\n";
                        ofs << "          \"move_speed\": " << pc->GetMoveSpeed();
                    } else if (auto* sky = dynamic_cast<SkydomeScript*>(sc)) {
                        ofs << ",\n";
                        ofs << "          \"rotation_speed\": " << sky->GetRotationSpeed() << ",\n";
                        const auto& col = sky->GetColor();
                        ofs << "          \"color\": [" << col.x << ", " << col.y << ", " << col.z << ", " << col.w << "],\n";
                        ofs << "          \"model_key\": \"" << sky->GetModelKey() << "\",\n";
                        ofs << "          \"texture_key\": \"" << sky->GetTextureKey() << "\"";
                    }
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

    return ofs.str();
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

    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    ofs << SerializeToString(sceneName, gameObjects);
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
    return DeserializeFromString(ss.str(), outSceneName, outGameObjects);
}

bool SceneSerializer::DeserializeFromString(
    const std::string& json,
    std::string& outSceneName,
    std::vector<std::unique_ptr<GameObject>>& outGameObjects) {

    // 1. シーン名の抽出
    outSceneName = ExtractString(json, "scene_name", "Untitled");

    // 2. GameObjectブロックの抽出
    auto goBlocks = ExtractObjectsFromArray(json, "game_objects");
    outGameObjects.clear();
    outGameObjects.reserve(goBlocks.size());

    for (const auto& goBlock : goBlocks) {
        std::string name = ExtractString(goBlock, "name", "GameObject");
        std::string uuid = ExtractString(goBlock, "uuid", "");
        bool visible = ExtractBool(goBlock, "visible", true);

        auto go = std::make_unique<GameObject>(name);
        if (!uuid.empty()) {
            go->SetUUID(uuid);
        }
        go->SetVisible(visible);

        // Transform の復元
        Vector3 pos = ExtractVector3(goBlock, "position", { 0.0f, 0.0f, 0.0f });
        Vector3 rot = ExtractVector3(goBlock, "rotation", { 0.0f, 0.0f, 0.0f });
        Vector3 scale = ExtractVector3(goBlock, "scale", { 1.0f, 1.0f, 1.0f });

        go->SetPosition(pos);
        go->SetRotate(rot);
        go->SetScale(scale);
        go->UpdateMatrix();

        // 3. コンポーネント群の抽出と復元
        auto compBlocks = ExtractObjectsFromArray(goBlock, "components");
        for (const auto& compBlock : compBlocks) {
            std::string type = ExtractString(compBlock, "type");

            if (type == "MeshRenderer") {
                auto* mr = go->AddComponent<MeshRendererComponent>();
                int meshType = ExtractInt(compBlock, "mesh_type", 0);
                mr->SetMeshType(static_cast<MeshRendererComponent::MeshType>(meshType));
                mr->SetTextureKey(ExtractString(compBlock, "texture_key", "white"));
                mr->SetModelKey(ExtractString(compBlock, "model_key", "cube"));
                mr->SetAnimationKey(ExtractString(compBlock, "animation_key", ""));
                mr->SetAnimationPlaying(ExtractBool(compBlock, "is_animation_playing", false));
                mr->SetShowSkeleton(ExtractBool(compBlock, "show_skeleton", true));
                mr->SetEnvMapKey(ExtractString(compBlock, "env_map_key", ""));
                mr->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));
                mr->SetLightingMode(ExtractInt(compBlock, "lighting_mode", 1));
                mr->SetShininess(ExtractFloat(compBlock, "shininess", 30.0f));
                mr->SetEnvironmentCoefficient(ExtractFloat(compBlock, "environment_coefficient", 1.0f));

                Transform off{};
                off.translate = ExtractVector3(compBlock, "offset_position", { 0.0f, 0.0f, 0.0f });
                off.rotate = ExtractVector3(compBlock, "offset_rotation", { 0.0f, 0.0f, 0.0f });
                off.scale = ExtractVector3(compBlock, "offset_scale", { 1.0f, 1.0f, 1.0f });
                mr->SetOffset(off);
            } else if (type == "SpriteRenderer") {
                auto* sr = go->AddComponent<SpriteRendererComponent>();
                int shapeType = ExtractInt(compBlock, "shape_type", 0);
                sr->SetShapeType(static_cast<SpriteRendererComponent::ShapeType>(shapeType));
                sr->SetTextureKey(ExtractString(compBlock, "texture_key", "white"));
                sr->SetSize(ExtractVector2(compBlock, "size", { 100.0f, 100.0f }));
                sr->SetRadius(ExtractFloat(compBlock, "radius", 50.0f));
                sr->SetInnerRadius(ExtractFloat(compBlock, "inner_radius", 35.0f));
                sr->SetLineStart(ExtractVector2(compBlock, "line_start", { 0.0f, 0.0f }));
                sr->SetLineEnd(ExtractVector2(compBlock, "line_end", { 150.0f, 0.0f }));
                sr->SetLineThickness(ExtractFloat(compBlock, "thickness", 4.0f));
                sr->SetColor(ExtractVector4(compBlock, "color", { 1.0f, 1.0f, 1.0f, 1.0f }));

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
            } else if (type == "AudioSource") {
                auto* as = go->AddComponent<AudioSourceComponent>();
                std::string soundPath = ExtractString(compBlock, "file_path", "");
                as->SetFilePath(soundPath);
                as->SetVolume(ExtractFloat(compBlock, "volume", 1.0f));
                as->SetLoop(ExtractBool(compBlock, "loop", false));
                as->SetAutoPlay(ExtractBool(compBlock, "auto_play", false));
                if (!soundPath.empty()) {
                    as->Load(soundPath);
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
                    } else if (auto* sky = dynamic_cast<SkydomeScript*>(script.get())) {
                        constexpr float kDefaultRotSpeedFallback = 0.002f;
                        float rotSpeed = ExtractFloat(compBlock, "rotation_speed", kDefaultRotSpeedFallback);
                        sky->SetRotationSpeed(rotSpeed);

                        constexpr Vector4 kDefaultColorFallback = { 1.0f, 1.0f, 1.0f, 1.0f };
                        Vector4 col = ExtractVector4(compBlock, "color", kDefaultColorFallback);
                        sky->SetColor(col);

                        const std::string kDefaultModelKeyFallback = "skydome";
                        std::string modelKey = ExtractString(compBlock, "model_key", kDefaultModelKeyFallback);
                        sky->SetModelKey(modelKey);

                        const std::string kDefaultTextureKeyFallback = "skydome";
                        std::string texKey = ExtractString(compBlock, "texture_key", kDefaultTextureKeyFallback);
                        sky->SetTextureKey(texKey);
                    }
                    go->AddComponentInstance(std::move(script));
                }
            }
        }

        outGameObjects.push_back(std::move(go));
    }

    // 4. 2パス目: スクリプト間・コンポーネント間の参照解決 (ResolveReferences)
    std::unordered_map<std::string, GameObject*> uuidMap;
    for (const auto& obj : outGameObjects) {
        if (obj) {
            uuidMap[obj->GetUUID()] = obj.get();
        }
    }

    for (const auto& obj : outGameObjects) {
        if (!obj) continue;
        for (const auto& comp : obj->GetComponents()) {
            if (auto* sc = dynamic_cast<ScriptComponent*>(comp.get())) {
                sc->ResolveReferences(uuidMap);
            }
        }
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

    auto* pe = go->AddComponent<GPUParticleComponent>();
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
    constexpr float kEpsilon = 0.001f;
    bool isPassed = true;

    if (loadedSceneName != "TestScene") isPassed = false;
    if (loadedObjects.size() != 1) isPassed = false;
    if (isPassed && loadedObjects[0]->GetName() != "TestEntity") isPassed = false;
    if (isPassed && std::abs(loadedObjects[0]->GetPosition().x - 1.0f) > kEpsilon) isPassed = false;
    if (isPassed && std::abs(loadedObjects[0]->GetPosition().y - 2.0f) > kEpsilon) isPassed = false;
    if (isPassed && std::abs(loadedObjects[0]->GetPosition().z - 3.0f) > kEpsilon) isPassed = false;
    if (isPassed && loadedObjects[0]->GetComponents().size() != 6) isPassed = false;
    if (isPassed && !loadedObjects[0]->HasComponent<MeshRendererComponent>()) isPassed = false;
    if (isPassed && !loadedObjects[0]->HasComponent<BoxColliderComponent>()) isPassed = false;
    if (isPassed && !loadedObjects[0]->HasComponent<AudioSourceComponent>()) isPassed = false;
    if (isPassed && !loadedObjects[0]->HasComponent<GPUParticleComponent>()) isPassed = false;
    if (isPassed && !loadedObjects[0]->HasComponent<TextRenderer2DComponent>()) isPassed = false;
    if (isPassed && !loadedObjects[0]->HasComponent<TextRenderer3DComponent>()) isPassed = false;

    if (isPassed) {
        auto* loadedTr3 = loadedObjects[0]->GetComponent<TextRenderer3DComponent>();
        if (!loadedTr3 || loadedTr3->GetText() != "SelfTest3D" || loadedTr3->GetBillboardMode() != TextObject3D::BillboardMode::YAxisOnly) {
            isPassed = false;
        }
    }

    // 5. テスト完了後のクリーンアップ（実リソースフォルダにテスト用ファイルを残さない）
    std::error_code ec;
    std::filesystem::remove(testPath, ec);

    return isPassed;
}
