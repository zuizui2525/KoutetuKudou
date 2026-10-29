#pragma once
#include "Engine/Math/MathStructs.h"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

/// <summary>
/// GPUパーティクルのエミッターおよびシミュレーションパラメータ
/// </summary>
struct GPUParticleSetting {
    // 識別・メタ情報
    std::string name = "Default";
    std::string textureKey = "white";

    // トランスフォーム & エミッター
    Vector3 emitterPosition = { 0.0f, 0.0f, 0.0f };
    Vector3 startScale      = { 0.35f, 0.35f, 0.35f };
    Vector3 endScale        = { 0.05f, 0.05f, 0.05f };

    // カラーグラデーション (RGBA)
    Vector4 startColor      = { 1.0f, 0.6f, 0.2f, 1.0f };
    Vector4 endColor        = { 1.0f, 0.1f, 0.0f, 0.0f };

    // 運動・初速範囲
    Vector3 minVelocity     = { -2.5f, 1.0f, -2.5f };
    Vector3 maxVelocity     = {  2.5f, 6.0f,  2.5f };

    // 寿命 (秒)
    float minLifeTime       = 0.8f;
    float maxLifeTime       = 2.0f;

    // 物理
    float gravity           = 3.5f;
    float killAtY           = -999.0f; // -900.0f 以下で無効

    // 発生制御
    float spawnRatePerSec   = 0.0f;     // 1秒あたりの自動発生数（初期状態は停止）
    bool  isAutoSpawning    = false;    // 自動連続発生を行うか（コンポーネントや明示呼び出しがあるまでOFF）
    uint32_t burstCount     = 1000;     // 手動バースト発生時の個数
    int   blendMode         = 1;        // 0: Normal(半透明), 1: Add(加算), 2: Subtract(減算), 3: Multiply(乗算)
};

/// <summary>
/// GPUパーティクルのビルトインプリセット群およびJSONシリアライザ
/// </summary>
class GPUParticlePresets {
public:
    // 定数定義 (マジックナンバー排除)
    static inline const std::string kParticleDirectory = "resources/Particles";
    static inline const std::string kFileExtension = ".json";

    // -------------------------------------------------------------
    // ビルトインプリセット生成関数
    // -------------------------------------------------------------

    /// <summary>
    /// <summary>
    /// 炎 (Fire): 鮮やかな橙から黄色、上昇気流で縮小しながら消滅
    /// </summary>
    static GPUParticleSetting CreateFire() {
        GPUParticleSetting s;
        s.name = "Fire";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 0.0f, 0.0f };
        s.startScale = { 0.35f, 0.35f, 0.35f };
        s.endScale = { 0.05f, 0.05f, 0.05f };
        s.startColor = { 1.0f, 0.7f, 0.1f, 1.0f };
        s.endColor = { 0.9f, 0.15f, 0.0f, 0.0f };
        s.minVelocity = { -0.5f, 1.0f, -0.5f };
        s.maxVelocity = {  0.5f, 3.0f,  0.5f };
        s.minLifeTime = 0.4f;
        s.maxLifeTime = 0.8f;
        s.gravity = -0.5f; // 浮力
        s.killAtY = -999.0f;
        s.spawnRatePerSec = 40.0f;
        s.isAutoSpawning = true; // デフォルトで自動連続発生ON
        s.burstCount = 30;
        return s;
    }

    /// <summary>
    /// 爆発 (Explosion): 球状に初速で拡散し、減速しながらフェードアウト
    /// </summary>
    static GPUParticleSetting CreateExplosion() {
        GPUParticleSetting s;
        s.name = "Explosion";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 0.5f, 0.0f };
        s.startScale = { 0.4f, 0.4f, 0.4f };
        s.endScale = { 0.02f, 0.02f, 0.02f };
        s.startColor = { 1.0f, 0.9f, 0.3f, 1.0f };
        s.endColor = { 0.8f, 0.1f, 0.0f, 0.0f };
        s.minVelocity = { -5.0f, -1.5f, -5.0f };
        s.maxVelocity = {  5.0f,  6.0f,  5.0f };
        s.minLifeTime = 0.3f;
        s.maxLifeTime = 0.7f;
        s.gravity = 4.0f;
        s.killAtY = 0.0f;
        s.spawnRatePerSec = 0.0f;
        s.isAutoSpawning = false; // バースト専用
        s.burstCount = 60;
        return s;
    }

    /// <summary>
    /// 煙 (Smoke): ゆったりと広がりながら上昇し、薄れて消える
    /// </summary>
    static GPUParticleSetting CreateSmoke() {
        GPUParticleSetting s;
        s.name = "Smoke";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 0.0f, 0.0f };
        s.startScale = { 0.2f, 0.2f, 0.2f };
        s.endScale = { 0.6f, 0.6f, 0.6f };
        s.startColor = { 0.4f, 0.4f, 0.45f, 0.5f };
        s.endColor = { 0.15f, 0.15f, 0.18f, 0.0f };
        s.minVelocity = { -0.3f, 0.5f, -0.3f };
        s.maxVelocity = {  0.3f, 1.5f,  0.3f };
        s.minLifeTime = 1.0f;
        s.maxLifeTime = 2.0f;
        s.gravity = -0.2f;
        s.killAtY = -999.0f;
        s.spawnRatePerSec = 20.0f;
        s.isAutoSpawning = true;
        s.burstCount = 20;
        s.blendMode = 0; // Normal(半透明)
        return s;
    }

    /// <summary>
    /// 火花 (Sparks): 極小で高速、地面に落ちて跳ねるような挙動
    /// </summary>
    static GPUParticleSetting CreateSparks() {
        GPUParticleSetting s;
        s.name = "Sparks";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 0.2f, 0.0f };
        s.startScale = { 0.08f, 0.08f, 0.08f };
        s.endScale = { 0.02f, 0.02f, 0.02f };
        s.startColor = { 1.0f, 0.95f, 0.8f, 1.0f };
        s.endColor = { 1.0f, 0.4f, 0.0f, 0.0f };
        s.minVelocity = { -4.0f, 2.0f, -4.0f };
        s.maxVelocity = {  4.0f, 6.0f,  4.0f };
        s.minLifeTime = 0.2f;
        s.maxLifeTime = 0.5f;
        s.gravity = 10.0f;
        s.killAtY = 0.0f;
        s.spawnRatePerSec = 30.0f;
        s.isAutoSpawning = true;
        s.burstCount = 30;
        return s;
    }

    /// <summary>
    /// 降雪 (Snow): 上空からゆっくりとひらひら舞い落ちる
    /// </summary>
    static GPUParticleSetting CreateSnow() {
        GPUParticleSetting s;
        s.name = "Snow";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 8.0f, 0.0f };
        s.startScale = { 0.12f, 0.12f, 0.12f };
        s.endScale = { 0.08f, 0.08f, 0.08f };
        s.startColor = { 0.9f, 0.95f, 1.0f, 0.9f };
        s.endColor = { 0.8f, 0.9f, 1.0f, 0.0f };
        s.minVelocity = { -0.5f, -1.5f, -0.5f };
        s.maxVelocity = {  0.5f, -0.5f,  0.5f };
        s.minLifeTime = 1.5f;
        s.maxLifeTime = 3.0f;
        s.gravity = 0.2f;
        s.killAtY = 0.0f;
        s.spawnRatePerSec = 30.0f;
        s.isAutoSpawning = true;
        s.burstCount = 30;
        s.blendMode = 0; // Normal(半透明)
        return s;
    }

    /// <summary>
    /// 魔法オーラ (Magic Aura): 幻想的なシアンと紫の光が螺旋状に立ち昇る
    /// </summary>
    static GPUParticleSetting CreateMagicAura() {
        GPUParticleSetting s;
        s.name = "MagicAura";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 0.0f, 0.0f };
        s.startScale = { 0.25f, 0.25f, 0.25f };
        s.endScale = { 0.04f, 0.04f, 0.04f };
        s.startColor = { 0.2f, 0.8f, 1.0f, 1.0f }; // シアン
        s.endColor = { 0.8f, 0.2f, 1.0f, 0.0f };   // 紫フェード
        s.minVelocity = { -1.0f, 1.0f, -1.0f };
        s.maxVelocity = {  1.0f, 2.5f,  1.0f };
        s.minLifeTime = 0.5f;
        s.maxLifeTime = 1.0f;
        s.gravity = -0.5f;
        s.killAtY = -999.0f;
        s.spawnRatePerSec = 30.0f;
        s.isAutoSpawning = true;
        s.burstCount = 30;
        return s;
    }

    /// <summary>
    /// 花火 (Fireworks): 上空で鮮やかに多色拡散する
    /// </summary>
    static GPUParticleSetting CreateFireworks() {
        GPUParticleSetting s;
        s.name = "Fireworks";
        s.textureKey = "white";
        s.emitterPosition = { 0.0f, 5.0f, 0.0f };
        s.startScale = { 0.25f, 0.25f, 0.25f };
        s.endScale = { 0.03f, 0.03f, 0.03f };
        s.startColor = { 1.0f, 0.3f, 0.7f, 1.0f }; // マゼンタ
        s.endColor = { 0.2f, 1.0f, 0.8f, 0.0f };   // エメラルドグリーン
        s.minVelocity = { -3.0f, -2.0f, -3.0f };
        s.maxVelocity = {  3.0f,  4.0f,  3.0f };
        s.minLifeTime = 0.6f;
        s.maxLifeTime = 1.2f;
        s.gravity = 2.0f;
        s.killAtY = 0.0f;
        s.spawnRatePerSec = 0.0f;
        s.isAutoSpawning = false;
        s.burstCount = 60;
        return s;
    }

    /// <summary>
    /// 弾丸軌道 (BulletTrail): 弾丸の後方に微細な粒子が短時間漂う軌跡エフェクト
    /// </summary>
    static GPUParticleSetting CreateBulletTrail() {
        GPUParticleSetting s;
        s.name = "BulletTrail";
        s.textureKey = "circle";
        s.emitterPosition = { 0.0f, 0.0f, 0.0f };
        s.startScale = { 0.16f, 0.16f, 0.16f };
        s.endScale = { 0.02f, 0.02f, 0.02f };
        s.startColor = { 0.35f, 0.85f, 1.0f, 0.95f }; // 発光シアンブルー
        s.endColor = { 0.10f, 0.30f, 0.95f, 0.0f };   // ディープブルー消滅
        s.minVelocity = { -0.05f, -0.05f, -0.05f };
        s.maxVelocity = {  0.05f,  0.05f,  0.05f };
        s.minLifeTime = 0.08f;
        s.maxLifeTime = 0.16f;
        s.gravity = 0.0f;
        s.killAtY = -999.0f;
        s.spawnRatePerSec = 50.0f;
        s.isAutoSpawning = true;
        s.burstCount = 1;
        return s;
    }

    /// <summary>
    /// 全ビルトインプリセット名一覧の取得
    /// </summary>
    static std::vector<std::string> GetPresetNames() {
        return { "BulletTrail", "Fire", "Explosion", "Smoke", "Sparks", "Snow", "MagicAura", "Fireworks" };
    }

    /// <summary>
    /// 名前からビルトインプリセットを取得
    /// </summary>
    static GPUParticleSetting GetPresetByName(const std::string& name) {
        if (name == "BulletTrail") return CreateBulletTrail();
        if (name == "Fire") return CreateFire();
        if (name == "Explosion") return CreateExplosion();
        if (name == "Smoke") return CreateSmoke();
        if (name == "Sparks") return CreateSparks();
        if (name == "Snow") return CreateSnow();
        if (name == "MagicAura") return CreateMagicAura();
        if (name == "Fireworks") return CreateFireworks();
        return CreateBulletTrail(); // デフォルト
    }

    // -------------------------------------------------------------
    // JSON 保存・読込
    // -------------------------------------------------------------

    /// <summary>
    /// 設定を JSON ファイルとして保存
    /// </summary>
    static bool SaveToJson(const GPUParticleSetting& setting, const std::string& filepath) {
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream ofs(filepath);
        if (!ofs.is_open()) return false;

        ofs << "{\n";
        ofs << "  \"name\": \"" << setting.name << "\",\n";
        ofs << "  \"textureKey\": \"" << setting.textureKey << "\",\n";
        ofs << "  \"emitterPosition\": [" << setting.emitterPosition.x << ", " << setting.emitterPosition.y << ", " << setting.emitterPosition.z << "],\n";
        ofs << "  \"startScale\": [" << setting.startScale.x << ", " << setting.startScale.y << ", " << setting.startScale.z << "],\n";
        ofs << "  \"endScale\": [" << setting.endScale.x << ", " << setting.endScale.y << ", " << setting.endScale.z << "],\n";
        ofs << "  \"startColor\": [" << setting.startColor.x << ", " << setting.startColor.y << ", " << setting.startColor.z << ", " << setting.startColor.w << "],\n";
        ofs << "  \"endColor\": [" << setting.endColor.x << ", " << setting.endColor.y << ", " << setting.endColor.z << ", " << setting.endColor.w << "],\n";
        ofs << "  \"minVelocity\": [" << setting.minVelocity.x << ", " << setting.minVelocity.y << ", " << setting.minVelocity.z << "],\n";
        ofs << "  \"maxVelocity\": [" << setting.maxVelocity.x << ", " << setting.maxVelocity.y << ", " << setting.maxVelocity.z << "],\n";
        ofs << "  \"minLifeTime\": " << setting.minLifeTime << ",\n";
        ofs << "  \"maxLifeTime\": " << setting.maxLifeTime << ",\n";
        ofs << "  \"gravity\": " << setting.gravity << ",\n";
        ofs << "  \"killAtY\": " << setting.killAtY << ",\n";
        ofs << "  \"spawnRatePerSec\": " << setting.spawnRatePerSec << ",\n";
        ofs << "  \"isAutoSpawning\": " << (setting.isAutoSpawning ? "true" : "false") << ",\n";
        ofs << "  \"burstCount\": " << setting.burstCount << ",\n";
        ofs << "  \"blendMode\": " << setting.blendMode << "\n";
        ofs << "}\n";

        return true;
    }

    /// <summary>
    /// JSON ファイルから設定を読み込み
    /// </summary>
    static bool LoadFromJson(const std::string& filepath, GPUParticleSetting& outSetting) {
        std::ifstream ifs(filepath);
        if (!ifs.is_open()) return false;

        std::stringstream buffer;
        buffer << ifs.rdbuf();
        std::string json = buffer.str();

        auto extractString = [](const std::string& src, const std::string& key) -> std::string {
            std::string token = "\"" + key + "\":";
            size_t pos = src.find(token);
            if (pos == std::string::npos) return "";
            size_t start = src.find('\"', pos + token.length());
            if (start == std::string::npos) return "";
            size_t end = src.find('\"', start + 1);
            if (end == std::string::npos) return "";
            return src.substr(start + 1, end - start - 1);
        };

        auto extractFloat = [](const std::string& src, const std::string& key, float defaultVal) -> float {
            std::string token = "\"" + key + "\":";
            size_t pos = src.find(token);
            if (pos == std::string::npos) return defaultVal;
            size_t start = pos + token.length();
            while (start < src.length() && (src[start] == ' ' || src[start] == '\t')) start++;
            try {
                return std::stof(src.substr(start));
            } catch (...) {
                return defaultVal;
            }
        };

        auto extractBool = [](const std::string& src, const std::string& key, bool defaultVal) -> bool {
            std::string token = "\"" + key + "\":";
            size_t pos = src.find(token);
            if (pos == std::string::npos) return defaultVal;
            size_t tPos = src.find("true", pos);
            size_t fPos = src.find("false", pos);
            if (tPos != std::string::npos && (fPos == std::string::npos || tPos < fPos)) return true;
            if (fPos != std::string::npos) return false;
            return defaultVal;
        };

        auto extractVec3 = [](const std::string& src, const std::string& key, const Vector3& defaultVal) -> Vector3 {
            std::string token = "\"" + key + "\":";
            size_t pos = src.find(token);
            if (pos == std::string::npos) return defaultVal;
            size_t start = src.find('[', pos);
            size_t end = src.find(']', start);
            if (start == std::string::npos || end == std::string::npos) return defaultVal;
            std::string content = src.substr(start + 1, end - start - 1);
            std::stringstream ss(content);
            std::string item;
            std::vector<float> vals;
            while (std::getline(ss, item, ',')) {
                try {
                    vals.push_back(std::stof(item));
                } catch (...) {}
            }
            if (vals.size() >= 3) {
                return { vals[0], vals[1], vals[2] };
            }
            return defaultVal;
        };

        auto extractVec4 = [](const std::string& src, const std::string& key, const Vector4& defaultVal) -> Vector4 {
            std::string token = "\"" + key + "\":";
            size_t pos = src.find(token);
            if (pos == std::string::npos) return defaultVal;
            size_t start = src.find('[', pos);
            size_t end = src.find(']', start);
            if (start == std::string::npos || end == std::string::npos) return defaultVal;
            std::string content = src.substr(start + 1, end - start - 1);
            std::stringstream ss(content);
            std::string item;
            std::vector<float> vals;
            while (std::getline(ss, item, ',')) {
                try {
                    vals.push_back(std::stof(item));
                } catch (...) {}
            }
            if (vals.size() >= 4) {
                return { vals[0], vals[1], vals[2], vals[3] };
            }
            return defaultVal;
        };

        outSetting.name = extractString(json, "name");
        if (outSetting.name.empty()) outSetting.name = "Custom";
        outSetting.textureKey = extractString(json, "textureKey");
        if (outSetting.textureKey.empty()) outSetting.textureKey = "white";

        outSetting.emitterPosition = extractVec3(json, "emitterPosition", outSetting.emitterPosition);
        outSetting.startScale = extractVec3(json, "startScale", outSetting.startScale);
        outSetting.endScale = extractVec3(json, "endScale", outSetting.endScale);
        outSetting.startColor = extractVec4(json, "startColor", outSetting.startColor);
        outSetting.endColor = extractVec4(json, "endColor", outSetting.endColor);
        outSetting.minVelocity = extractVec3(json, "minVelocity", outSetting.minVelocity);
        outSetting.maxVelocity = extractVec3(json, "maxVelocity", outSetting.maxVelocity);

        outSetting.minLifeTime = extractFloat(json, "minLifeTime", outSetting.minLifeTime);
        outSetting.maxLifeTime = extractFloat(json, "maxLifeTime", outSetting.maxLifeTime);
        outSetting.gravity = extractFloat(json, "gravity", outSetting.gravity);
        outSetting.killAtY = extractFloat(json, "killAtY", outSetting.killAtY);
        outSetting.spawnRatePerSec = extractFloat(json, "spawnRatePerSec", outSetting.spawnRatePerSec);
        outSetting.isAutoSpawning = extractBool(json, "isAutoSpawning", outSetting.isAutoSpawning);
        outSetting.burstCount = static_cast<uint32_t>(extractFloat(json, "burstCount", static_cast<float>(outSetting.burstCount)));
        outSetting.blendMode = static_cast<int>(extractFloat(json, "blendMode", static_cast<float>(outSetting.blendMode)));

        return true;
    }
};
