#define NOMINMAX
#include "App/Scene/Core/SceneTransitionManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"
#include "Engine/Base/Log/Log.h"
#ifdef _USEIMGUI
#include <imgui.h>
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <format>

namespace {
    // 定数定義 (マジックナンバー・マジックストリング排除)
    constexpr const char* kTransitionsConfigFilePath = "resources/Config/Transitions.json";
    constexpr float kEpsilon = 0.0001f;
    constexpr float kColorScale = 255.0f;
    constexpr int32_t kMinGaussianRadius = 1;

    // 単体設定をストリームに書き込むヘルパー
    void WriteSingleSetting(std::ofstream& ofs, const std::string& prefix, const SingleTransitionSetting& s) {
        ofs << prefix << "enabled=" << (s.enabled ? 1 : 0) << "\n";
        ofs << prefix << "type=" << static_cast<int32_t>(s.effectType) << "\n";
        ofs << prefix << "duration=" << s.duration << "\n";
        ofs << prefix << "colorR=" << s.color.x << "\n";
        ofs << prefix << "colorG=" << s.color.y << "\n";
        ofs << prefix << "colorB=" << s.color.z << "\n";
        ofs << prefix << "colorA=" << s.color.w << "\n";
        ofs << prefix << "blurStrength=" << s.blurStrength << "\n";
        ofs << prefix << "edgeColorR=" << s.edgeColor.x << "\n";
        ofs << prefix << "edgeColorG=" << s.edgeColor.y << "\n";
        ofs << prefix << "edgeColorB=" << s.edgeColor.z << "\n";
        ofs << prefix << "edgeWidth=" << s.edgeWidth << "\n";
    }

    // 単体設定をキー・値からパースするヘルパー
    void ParseSingleSetting(const std::string& key, const std::string& val, const std::string& prefix, SingleTransitionSetting& s) {
        if (key.rfind(prefix, 0) != 0) return;
        std::string subKey = key.substr(prefix.length());
        try {
            if (subKey == "enabled") s.enabled = (std::stoi(val) != 0);
            else if (subKey == "type") s.effectType = static_cast<TransitionEffectType>(std::stoi(val));
            else if (subKey == "duration") s.duration = std::stof(val);
            else if (subKey == "colorR") s.color.x = std::stof(val);
            else if (subKey == "colorG") s.color.y = std::stof(val);
            else if (subKey == "colorB") s.color.z = std::stof(val);
            else if (subKey == "colorA") s.color.w = std::stof(val);
            else if (subKey == "blurStrength") s.blurStrength = std::stof(val);
            else if (subKey == "edgeColorR") s.edgeColor.x = std::stof(val);
            else if (subKey == "edgeColorG") s.edgeColor.y = std::stof(val);
            else if (subKey == "edgeColorB") s.edgeColor.z = std::stof(val);
            else if (subKey == "edgeWidth") s.edgeWidth = std::stof(val);
        } catch (...) {}
    }
}

SceneTransitionManager* SceneTransitionManager::GetInstance() {
    static SceneTransitionManager instance;
    return &instance;
}

void SceneTransitionManager::Initialize() {
    sceneConfigs_.clear();

    if (std::filesystem::exists(kTransitionsConfigFilePath)) {
        std::ifstream ifs(kTransitionsConfigFilePath);
        if (ifs.is_open()) {
            std::string line;
            std::string currentScene;
            SceneTransitionConfig config;

            while (std::getline(ifs, line)) {
                if (line.rfind("[", 0) == 0 && line.find("]") != std::string::npos) {
                    if (!currentScene.empty()) {
                        sceneConfigs_[currentScene] = config;
                    }
                    currentScene = line.substr(1, line.find("]") - 1);
                    config = SceneTransitionConfig(); // リセット
                } else if (!currentScene.empty()) {
                    std::stringstream ss(line);
                    std::string key;
                    if (std::getline(ss, key, '=')) {
                        std::string val;
                        std::getline(ss, val);
                        ParseSingleSetting(key, val, "out_", config.outSetting);
                        ParseSingleSetting(key, val, "in_", config.inSetting);

                        // 旧フォーマットの後方互換フォールバック
                        if (key == "type" && !config.outSetting.enabled && !config.inSetting.enabled) {
                            int t = std::stoi(val);
                            if (t != 0) {
                                config.outSetting.enabled = true;
                                config.outSetting.effectType = static_cast<TransitionEffectType>(t);
                                config.inSetting.enabled = true;
                                config.inSetting.effectType = static_cast<TransitionEffectType>(t);
                            }
                        }
                    }
                }
            }
            if (!currentScene.empty()) {
                sceneConfigs_[currentScene] = config;
            }
        }
    }
}

void SceneTransitionManager::SaveConfigToFile(const std::string& sceneName) {
    if (!sceneConfigs_.contains(sceneName)) return;

    std::filesystem::path dir = std::filesystem::path(kTransitionsConfigFilePath).parent_path();
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    std::ofstream ofs(kTransitionsConfigFilePath);
    if (!ofs.is_open()) return;

    for (const auto& [name, cfg] : sceneConfigs_) {
        ofs << "[" << name << "]\n";
        WriteSingleSetting(ofs, "out_", cfg.outSetting);
        WriteSingleSetting(ofs, "in_", cfg.inSetting);
        ofs << "\n";
    }
}

void SceneTransitionManager::LoadConfigFromFile(const std::string& sceneName) {
    Initialize();
}

SceneTransitionConfig SceneTransitionManager::GetConfig(const std::string& sceneName) {
    if (sceneConfigs_.contains(sceneName)) {
        return sceneConfigs_[sceneName];
    }
    return SceneTransitionConfig();
}

void SceneTransitionManager::SetConfig(const std::string& sceneName, const SceneTransitionConfig& config) {
    sceneConfigs_[sceneName] = config;
    SaveConfigToFile(sceneName);
}

void SceneTransitionManager::StartTransition(const std::string& fromSceneName, const std::string& toSceneName) {
    pendingSceneName_ = toSceneName;

    activeOutSetting_ = GetConfig(fromSceneName).outSetting;
    activeInSetting_ = GetConfig(toSceneName).inSetting;

    bool hasOut = activeOutSetting_.IsActive();
    bool hasIn = activeInSetting_.IsActive();

    // どちらの演出も無効なら即座に切り替え
    if (!hasOut && !hasIn) {
        if (onSceneSwitchCallback_) {
            onSceneSwitchCallback_(pendingSceneName_);
        }
        currentPhase_ = TransitionPhase::Idle;
        currentProgress_ = kMinProgress;
        return;
    }

    // OUT演出がある場合はFadeOutから開始
    if (hasOut) {
        currentActiveSetting_ = activeOutSetting_;
        currentPhase_ = TransitionPhase::FadeOut;
        timer_ = kZeroTime;
        currentProgress_ = kMinProgress;
    } else {
        // OUT演出がなくIN演出のみの場合はロード画面（Loadingステート）を挟んでからFadeInへ移行
        currentPhase_ = TransitionPhase::Loading;
        timer_ = kZeroTime;
        currentProgress_ = kMaxProgress;
        if (onSceneSwitchCallback_) {
            onSceneSwitchCallback_(pendingSceneName_);
        }
    }
}

void SceneTransitionManager::TriggerPreviewOut(const SingleTransitionSetting& setting) {
    activeOutSetting_ = setting;
    activeInSetting_ = SingleTransitionSetting(); // INなし
    currentActiveSetting_ = setting;

    if (!currentActiveSetting_.IsActive() || currentActiveSetting_.duration <= kEpsilon) {
        return;
    }

    currentPhase_ = TransitionPhase::PreviewFadeOut;
    timer_ = kZeroTime;
    currentProgress_ = kMinProgress;
}

void SceneTransitionManager::TriggerPreviewIn(const SingleTransitionSetting& setting) {
    activeOutSetting_ = SingleTransitionSetting(); // OUTなし
    activeInSetting_ = setting;
    currentActiveSetting_ = setting;

    if (!currentActiveSetting_.IsActive() || currentActiveSetting_.duration <= kEpsilon) {
        return;
    }

    currentPhase_ = TransitionPhase::PreviewFadeIn;
    timer_ = kZeroTime;
    currentProgress_ = kMaxProgress;
}

void SceneTransitionManager::TriggerPreviewFull(const SingleTransitionSetting& outSetting, const SingleTransitionSetting& inSetting) {
    activeOutSetting_ = outSetting;
    activeInSetting_ = inSetting;

    if (activeOutSetting_.IsActive()) {
        currentActiveSetting_ = activeOutSetting_;
        currentPhase_ = TransitionPhase::PreviewFadeOut;
        timer_ = kZeroTime;
        currentProgress_ = kMinProgress;
    } else if (activeInSetting_.IsActive()) {
        currentActiveSetting_ = activeInSetting_;
        currentPhase_ = TransitionPhase::PreviewFadeIn;
        timer_ = kZeroTime;
        currentProgress_ = kMaxProgress;
    }
}

bool SceneTransitionManager::IsTransitioning() const {
    return currentPhase_ != TransitionPhase::Idle;
}

float SceneTransitionManager::CalculateEaseInOut(float t) const {
    float clamped = (std::clamp)(t, kMinProgress, kMaxProgress);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

void SceneTransitionManager::Update(float deltaTime) {
    if (currentPhase_ == TransitionPhase::Idle) return;

    timer_ += deltaTime;
    float currentDuration = (std::max)(kEpsilon, currentActiveSetting_.duration);

    if (currentPhase_ == TransitionPhase::FadeOut || currentPhase_ == TransitionPhase::PreviewFadeOut) {
        float ratio = (std::clamp)(timer_ / currentDuration, kMinProgress, kMaxProgress);
        currentProgress_ = CalculateEaseInOut(ratio);
        ApplyPostEffects(currentProgress_, currentActiveSetting_, true);

        // 前半終了（完全暗転 / ピーク到達）
        if (timer_ >= currentDuration) {
            timer_ = kZeroTime;

            if (currentPhase_ == TransitionPhase::FadeOut) {
                // 完全暗転・ロード画面（Loadingステート）へ移行
                currentPhase_ = TransitionPhase::Loading;
                timer_ = kZeroTime;
                currentProgress_ = kMaxProgress;

                // シーン入替を予約（完全暗転の裏で実行される）
                if (onSceneSwitchCallback_) {
                    onSceneSwitchCallback_(pendingSceneName_);
                }
            } else {
                // 通しプレビューでIN演出があればPreviewFadeInへ移行、なければ終了
                if (activeInSetting_.IsActive()) {
                    currentActiveSetting_ = activeInSetting_;
                    currentPhase_ = TransitionPhase::PreviewFadeIn;
                    currentProgress_ = kMaxProgress;
                    ApplyPostEffects(currentProgress_, currentActiveSetting_, false);
                } else {
                    ResetPostEffects();
                    currentPhase_ = TransitionPhase::Idle;
                    currentProgress_ = kMinProgress;
                }
            }
        }
    }
    else if (currentPhase_ == TransitionPhase::Loading) {
        // 設定された最低ロード待機時間（Now Loading... 表示時間）を満たすまで待機
        if (timer_ < kDefaultLoadingDuration) {
            return;
        }

        // 最低ロード待機時間を満たし、新シーンの初期化完了後にFadeInへ移行
        if (activeInSetting_.IsActive()) {
            currentActiveSetting_ = activeInSetting_;
            currentPhase_ = TransitionPhase::FadeIn;
            timer_ = kZeroTime;
            currentProgress_ = kMaxProgress;
            ApplyPostEffects(currentProgress_, currentActiveSetting_, false);
        } else {
            ResetPostEffects();
            currentPhase_ = TransitionPhase::Idle;
            currentProgress_ = kMinProgress;
        }
    }
    else if (currentPhase_ == TransitionPhase::FadeIn || currentPhase_ == TransitionPhase::PreviewFadeIn) {
        float ratio = (std::clamp)(timer_ / currentDuration, kMinProgress, kMaxProgress);
        currentProgress_ = CalculateEaseInOut(kMaxProgress - ratio);
        ApplyPostEffects(currentProgress_, currentActiveSetting_, false);

        // 後半終了（通常動作へ復帰）
        if (timer_ >= currentDuration) {
            timer_ = kZeroTime;
            currentProgress_ = kMinProgress;
            ResetPostEffects();
            currentPhase_ = TransitionPhase::Idle;
        }
    }
}

void SceneTransitionManager::ApplyPostEffects(float progress, const SingleTransitionSetting& setting, bool isFadeOut) {
    if (!postProcess_) return;

    switch (setting.effectType) {
    case TransitionEffectType::Dissolve:
        postProcess_->SetDissolveActive(true);
        postProcess_->SetDissolveParams(progress, setting.edgeWidth, setting.edgeColor);
        break;

    case TransitionEffectType::RadialBlur:
        postProcess_->SetRadialBlurActive(true);
        postProcess_->SetRadialBlurParams(
            { 0.5f, 0.5f },
            progress * TransitionConstants::kMaxRadialBlurWidth * setting.blurStrength
        );
        break;

    case TransitionEffectType::GaussianBlur: {
        postProcess_->SetGaussianBlurActive(true);
        int32_t radius = (std::max)(kMinGaussianRadius, static_cast<int32_t>(progress * TransitionConstants::kMaxGaussianRadius * setting.blurStrength));
        float sigma = progress * TransitionConstants::kMaxGaussianSigma * setting.blurStrength;
        postProcess_->SetGaussianBlurParams(radius, sigma);
        break;
    }

    case TransitionEffectType::TVNoise:
        postProcess_->SetTVNoiseActive(true);
        postProcess_->SetTVNoiseStrength(progress * setting.blurStrength);
        break;

    case TransitionEffectType::GrayscaleFade:
        postProcess_->SetGrayscaleActive(progress > kEpsilon);
        break;

    default:
        break;
    }
}

void SceneTransitionManager::ResetPostEffects() {
    if (postProcess_) {
        // トランジションで有効化していたエフェクトのみを解除（シーン固有のVignette等は維持）
        postProcess_->SetDissolveActive(false);
        postProcess_->SetRadialBlurActive(false);
        postProcess_->SetGaussianBlurActive(false);
        postProcess_->SetTVNoiseActive(false);
        postProcess_->SetGrayscaleActive(false);
    }
}

#ifdef _USEIMGUI
void SceneTransitionManager::DrawEditorOverlay(ImDrawList* drawList, const Vector2& posMin, const Vector2& posMax) {
    if (!drawList || currentPhase_ == TransitionPhase::Idle) return;

    float width = posMax.x - posMin.x;
    float height = posMax.y - posMin.y;
    if (width <= 0.0f || height <= 0.0f) return;

    // Loading フェーズ: 完全暗転（黒板）
    if (currentPhase_ == TransitionPhase::Loading) {
        constexpr ImU32 kOpaqueBlack = IM_COL32(0, 0, 0, 255);
        drawList->AddRectFilled(ImVec2(posMin.x, posMin.y), ImVec2(posMax.x, posMax.y), kOpaqueBlack);
        return;
    }

    // 設定された最大アルファ値と進行度を乗算
    float currentAlpha = (std::clamp)(currentActiveSetting_.color.w * currentProgress_, kMinProgress, kMaxProgress);
    
    uint32_t r = static_cast<uint32_t>((std::clamp)(currentActiveSetting_.color.x, 0.0f, 1.0f) * kColorScale);
    uint32_t g = static_cast<uint32_t>((std::clamp)(currentActiveSetting_.color.y, 0.0f, 1.0f) * kColorScale);
    uint32_t b = static_cast<uint32_t>((std::clamp)(currentActiveSetting_.color.z, 0.0f, 1.0f) * kColorScale);
    uint32_t a = static_cast<uint32_t>(currentAlpha * kColorScale);
    ImU32 overlayColor = IM_COL32(r, g, b, a);

    switch (currentActiveSetting_.effectType) {
    case TransitionEffectType::ColorFade:
        if (a > 0) {
            drawList->AddRectFilled(ImVec2(posMin.x, posMin.y), ImVec2(posMax.x, posMax.y), overlayColor);
        }
        break;

    case TransitionEffectType::WipeLeft: {
        float wipeX = posMax.x - width * currentProgress_;
        drawList->AddRectFilled(ImVec2(wipeX, posMin.y), ImVec2(posMax.x, posMax.y), overlayColor);
        break;
    }

    case TransitionEffectType::WipeRight: {
        float wipeX = posMin.x + width * currentProgress_;
        drawList->AddRectFilled(ImVec2(posMin.x, posMin.y), ImVec2(wipeX, posMax.y), overlayColor);
        break;
    }

    default:
        // ポストプロセス系演出（TVNoise, RadialBlur, GaussianBlur, Dissolve, GrayscaleFade）でも、
        // 設定カラー（colorA > 0）がある場合はアルファブレンドで重ねて滑らかに暗転・明転を接続する
        if (a > 0) {
            drawList->AddRectFilled(ImVec2(posMin.x, posMin.y), ImVec2(posMax.x, posMax.y), overlayColor);
        }
        break;
    }
}
#endif
