
#include "Engine/Component/Components/GPUParticleComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleSetting.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include <algorithm>

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用定数 (マジックナンバー排除)
    constexpr size_t kNameBufferSize = 128;
    constexpr float kDragSpeed = 0.05f;
    constexpr float kSpawnRateMin = 0.0f;
    constexpr float kSpawnRateMax = 500.0f;
    constexpr uint32_t kDefaultBurstCount = 30;
}

GPUParticleComponent::GPUParticleComponent() = default;

GPUParticleComponent::~GPUParticleComponent() {
    ReleaseParticleObject();
}

void GPUParticleComponent::Initialize() {
    AcquireParticleObject();
    ApplyPreset(presetName_);
}

void GPUParticleComponent::AcquireParticleObject() {
    if (particleObject_) return;

    if (!sParticlePool_.empty()) {
        particleObject_ = std::move(sParticlePool_.back());
        sParticlePool_.pop_back();
        if (particleObject_) {
            particleObject_->Reset();
        }
    } else {
        particleObject_ = std::make_unique<GPUParticleObject>();
        particleObject_->Initialize(kDefaultComponentMaxParticles);
    }
}

void GPUParticleComponent::ReleaseParticleObject() {
    if (particleObject_) {
        // GPUパイプラインストール (FlushGPU) を防ぐため、破棄せずプールへ返却して再利用
        particleObject_->Reset();

        sParticlePool_.push_back(std::move(particleObject_));
    }
}

void GPUParticleComponent::ClearPool() {
    sParticlePool_.clear();
}

void GPUParticleComponent::ApplyPreset(const std::string& presetName) {
    presetName_ = presetName;
    if (!particleObject_) return;

    GPUParticleSetting setting;
    std::string jsonPath = GPUParticlePresets::kParticleDirectory + "/" + presetName + GPUParticlePresets::kFileExtension;
    if (!GPUParticlePresets::LoadFromJson(jsonPath, setting)) {
        setting = GPUParticlePresets::GetPresetByName(presetName);
    }

    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (isFirework) {
        // 花火モードでは通常パーティクルの自動発生を無効化（足元からのゴミパーティクル放出を完全防止）
        isAutoSpawning_ = false;
        spawnRatePerSec_ = 0.0f;
    }

    setting.isAutoSpawning = isAutoSpawning_;
    setting.spawnRatePerSec = isAutoSpawning_ ? spawnRatePerSec_ : 0.0f;
    textureKey_ = setting.textureKey;

    size_t bmIdx = static_cast<size_t>(setting.blendMode);
    if (bmIdx < static_cast<size_t>(ParticleBlendMode::Count)) {
        blendMode_ = static_cast<ParticleBlendMode>(bmIdx);
    }

    minLifeTime_ = setting.minLifeTime;
    maxLifeTime_ = setting.maxLifeTime;
    gravity_ = setting.gravity;
    startColor_ = setting.startColor;
    endColor_ = setting.endColor;
    if (setting.burstCount > 0) {
        burstCount_ = setting.burstCount;
    }

    particleObject_->ApplySetting(setting);

    if (isFirework) {
        const auto& points = textSampler_.ExtractPoints(fireworkText_);
        particleObject_->SetShapePoints(points);
        particleObject_->SetShapeType(1);
        particleObject_->SetBurstSpeed(textScale_);
        fireworkPhase_ = isLoop_ ? FireworkPhase::Launching : FireworkPhase::Idle;
        launchProgress_ = 0.0f;
        currentLaunchOffsetY_ = 0.0f;
    } else {
        particleObject_->SetShapeType(0);
        particleObject_->SetBurstSpeed(0.0f);
        currentLaunchOffsetY_ = 0.0f;
    }
}

void GPUParticleComponent::SetPresetName(const std::string& name) {
    ApplyPreset(name);
}

void GPUParticleComponent::SetBlendMode(ParticleBlendMode mode) {
    blendMode_ = mode;
    if (particleObject_) {
        particleObject_->SetBlendMode(mode);
    }
}

void GPUParticleComponent::SetAutoSpawning(bool active) {
    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (isFirework) {
        active = false;
    }
    isAutoSpawning_ = active;
    if (particleObject_) {
        auto& s = particleObject_->GetSetting();
        s.isAutoSpawning = active;
        s.spawnRatePerSec = active ? spawnRatePerSec_ : 0.0f;
    }
}

void GPUParticleComponent::SetSpawnRate(float rate) {
    spawnRatePerSec_ = rate;
    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (particleObject_ && isAutoSpawning_ && !isFirework) {
        particleObject_->GetSetting().spawnRatePerSec = rate;
    }
}

void GPUParticleComponent::Play() {
    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (isFirework) {
        fireworkPhase_ = FireworkPhase::Launching;
        launchProgress_ = 0.0f;
        currentLaunchOffsetY_ = 0.0f;
        burstTimer_ = 0.0f;
        SetAutoSpawning(false);
        if (particleObject_) particleObject_->Reset();
        return;
    }
    if (isLoop_) {
        SetAutoSpawning(true);
    } else {
        EmitBurst(burstCount_);
    }
}

void GPUParticleComponent::Stop() {
    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (isFirework) {
        fireworkPhase_ = FireworkPhase::Idle;
        currentLaunchOffsetY_ = 0.0f;
        burstTimer_ = 0.0f;
        SetAutoSpawning(false);
        if (particleObject_) particleObject_->Reset();
        return;
    }
    SetAutoSpawning(false);
    burstTimer_ = 0.0f;
    if (particleObject_) {
        particleObject_->Reset();
    }
}

void GPUParticleComponent::SetLoop(bool loop) {
    isLoop_ = loop;
    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (!isFirework) {
        SetAutoSpawning(loop);
    } else {
        SetAutoSpawning(false);
    }
}

void GPUParticleComponent::SetMinLifeTime(float life) {
    minLifeTime_ = life;
    if (particleObject_) {
        particleObject_->GetSetting().minLifeTime = life;
    }
}

void GPUParticleComponent::SetMaxLifeTime(float life) {
    maxLifeTime_ = life;
    if (particleObject_) {
        particleObject_->GetSetting().maxLifeTime = life;
    }
}

void GPUParticleComponent::SetGravity(float g) {
    gravity_ = g;
    if (particleObject_) {
        particleObject_->GetSetting().gravity = g;
    }
}

void GPUParticleComponent::SetStartColor(const Vector4& color) {
    startColor_ = color;
    if (particleObject_) {
        particleObject_->GetSetting().startColor = color;
    }
}

void GPUParticleComponent::SetEndColor(const Vector4& color) {
    endColor_ = color;
    if (particleObject_) {
        particleObject_->GetSetting().endColor = color;
    }
}

void GPUParticleComponent::EmitBurst(uint32_t count) {
    if (!particleObject_) return;
    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (isFirework) {

        const auto& pts = textSampler_.ExtractPoints(fireworkText_);
        particleObject_->SetShapePoints(pts);
        particleObject_->SetShapeType(1);
        particleObject_->SetBurstSpeed(textScale_);

        // 点群数（文字数）に応じた最適な粒子スケールを自動設定（潰れ・重なりを防止）
        float autoScale = 0.22f;
        if (pts.size() > 600) {
            autoScale = 0.12f;
        } else if (pts.size() > 300) {
            autoScale = 0.16f;
        }
        particleObject_->GetSetting().startScale = { autoScale, autoScale, autoScale };
        particleObject_->GetSetting().endScale = { autoScale * 0.15f, autoScale * 0.15f, autoScale * 0.15f };

        currentLaunchOffsetY_ = launchHeight_;
        fireworkPhase_ = FireworkPhase::Exploded;
        burstTimer_ = 0.0f;
        constexpr uint32_t kParticlesPerPoint = 3;
        uint32_t actualCount = (count > 0 && count != burstCount_) ? count : (std::max)(kFireworkBurstCount, static_cast<uint32_t>(pts.size() * kParticlesPerPoint));
        particleObject_->EmitBurst(actualCount);
        return;
    }
    particleObject_->SetShapeType(0);
    if (particleObject_) {
        particleObject_->EmitBurst(count);
    }
}

void GPUParticleComponent::Update() {
    if (!isActive_ || !particleObject_) return;

    Vector3 worldPos = offset_;
    if (owner_) {
        const Vector3& pos = owner_->GetPosition();
        worldPos.x += pos.x;
        worldPos.y += pos.y;
        worldPos.z += pos.z;
    }

    bool isFirework = (presetName_ == "TextFirework" || presetName_ == "Fireworks");
    if (isFirework) {
        // 文字花火専用の打ち上げ・炸裂シーケンス（常時自動発生は完全に無効化）
        if (particleObject_->GetSetting().isAutoSpawning) {
            particleObject_->GetSetting().isAutoSpawning = false;
            particleObject_->GetSetting().spawnRatePerSec = 0.0f;
            isAutoSpawning_ = false;
        }

        if (fireworkPhase_ == FireworkPhase::Launching) {
            launchProgress_ += kDefaultDeltaTime / kDefaultLaunchDuration;
            float t = (std::min)(launchProgress_, 1.0f);
            constexpr float kHalfPi = 1.5707963f;
            float easeY = std::sin(t * kHalfPi);
            currentLaunchOffsetY_ = easeY * launchHeight_;



            // 上昇中（ヒューンと上がる光の玉・火の粉）
            // 頂点手前（0.92）まで極短寿命の光る火花を放出し、頂点到達時には完全に自然消滅させる
            constexpr float kLaunchTrailCutoff = 0.92f;
            if (launchProgress_ < kLaunchTrailCutoff) {
                particleObject_->SetShapeType(0);
                auto& s = particleObject_->GetSetting();
                s.minLifeTime = 0.05f;
                s.maxLifeTime = 0.10f;
                s.minVelocity = { -0.3f, -1.0f, -0.3f };
                s.maxVelocity = {  0.3f, -0.2f,  0.3f };
                s.startScale = { 0.28f, 0.28f, 0.28f };
                s.endScale = { 0.02f, 0.02f, 0.02f };
                s.startColor = { 1.0f, 0.9f, 0.5f, 1.0f }; // 黄金に輝く上昇光
                s.endColor = { 1.0f, 0.3f, 0.0f, 0.0f };
                constexpr uint32_t kTrailCount = 2;
                particleObject_->EmitBurst(kTrailCount);
            }

            if (launchProgress_ >= 1.0f) {
                fireworkPhase_ = FireworkPhase::Exploded;
                burstTimer_ = 0.0f;
                currentLaunchOffsetY_ = launchHeight_;

                const auto& pts = textSampler_.ExtractPoints(fireworkText_);
                particleObject_->SetShapePoints(pts);
                particleObject_->SetShapeType(1);
                particleObject_->SetBurstSpeed(textScale_);
                auto& s = particleObject_->GetSetting();
                s.minLifeTime = minLifeTime_;
                s.maxLifeTime = maxLifeTime_;
                s.gravity = gravity_;
                s.startColor = startColor_;
                s.endColor = endColor_;

                // 点群数（文字数）に応じた最適な粒子スケールを自動設定（潰れ・重なりを防止）
                float autoScale = 0.22f;
                if (pts.size() > 600) {
                    autoScale = 0.12f;
                } else if (pts.size() > 300) {
                    autoScale = 0.16f;
                }
                s.startScale = { autoScale, autoScale, autoScale };
                s.endScale = { autoScale * 0.15f, autoScale * 0.15f, autoScale * 0.15f };

                constexpr uint32_t kParticlesPerPoint = 3;
                uint32_t count = (std::max)(kFireworkBurstCount, static_cast<uint32_t>(pts.size() * kParticlesPerPoint));
                particleObject_->EmitBurst(count);
            }
        } else if (fireworkPhase_ == FireworkPhase::Exploded) {
            burstTimer_ += kDefaultDeltaTime;
            float totalCycleTime = maxLifeTime_ + burstInterval_;
            if (isLoop_ && burstTimer_ >= totalCycleTime) {
                fireworkPhase_ = FireworkPhase::Launching;
                launchProgress_ = 0.0f;
                currentLaunchOffsetY_ = 0.0f;
                burstTimer_ = 0.0f;
            }
        }
        worldPos.y += currentLaunchOffsetY_;
    } else {
        // 通常パーティクルの単発・周期バーストのタイマー更新
    if (!isLoop_ && burstInterval_ > 0.0f) {
        burstTimer_ += kDefaultDeltaTime;
        if (burstTimer_ >= burstInterval_) {
            burstTimer_ = 0.0f;
            EmitBurst(burstCount_);
        }
    }

    }

    particleObject_->SetPosition(worldPos);
    particleObject_->Update(kDefaultDeltaTime);
}

void GPUParticleComponent::Draw() {
    if (!isActive_ || !particleObject_) return;

    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            auto* cmdList = dxCommon->GetCommandList();
            particleObject_->DispatchCompute(cmdList);
            particleObject_->Draw(cmdList, textureKey_, blendMode_);
        }
    }
}

void GPUParticleComponent::DrawInspector() {
#ifdef _USEIMGUI
    constexpr ImVec4 kHeaderColor = { 0.4f, 0.8f, 1.0f, 1.0f };
    ImGui::TextColored(kHeaderColor, "【GPUパーティクルコンポーネント】");

    // 1. プリセット選択
    if (ImGui::BeginCombo("Preset##GPUParticleComp", presetName_.c_str())) {
        auto presets = GPUParticlePresets::GetPresetNames();
        for (const auto& preset : presets) {
            bool isSelected = (presetName_ == preset);
            if (ImGui::Selectable(preset.c_str(), isSelected)) {
                ApplyPreset(preset);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // 2. ブレンドモード選択
    const char* blendNames[] = { "Normal (半透明)", "Add (加算発光)", "Subtract (減算)", "Multiply (乗算)" };
    int currentBlend = static_cast<int>(blendMode_);
    if (ImGui::Combo("Blend Mode##GPUParticleComp", &currentBlend, blendNames, IM_ARRAYSIZE(blendNames))) {
        SetBlendMode(static_cast<ParticleBlendMode>(currentBlend));
    }

    // 3. テクスチャキー
    char texBuf[kNameBufferSize]{};
    strncpy_s(texBuf, textureKey_.c_str(), sizeof(texBuf) - 1);
    if (ImGui::InputText("Texture Key##GPUParticleComp", texBuf, sizeof(texBuf))) {
        textureKey_ = texBuf;
    }

    // 3. オフセット
    float offset[3] = { offset_.x, offset_.y, offset_.z };
    if (ImGui::DragFloat3("Offset##GPUParticleComp", offset, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
        offset_ = { offset[0], offset[1], offset[2] };
    }

    // 4. ループ再生 & 発生間隔 / レート
    bool loop = isLoop_;
    if (ImGui::Checkbox("ループ再生 (Loop)##GPUParticleComp", &loop)) {
        SetLoop(loop);
    }

    if (isLoop_) {
        float rate = spawnRatePerSec_;
        if (ImGui::DragFloat("発生密度 (Spawn Rate/s)##GPUParticleComp", &rate, 1.0f, kSpawnRateMin, kSpawnRateMax, "%.1f/s")) {
            SetSpawnRate(rate);
        }
    } else {
        float interval = burstInterval_;
        if (ImGui::DragFloat("発射間隔 (Interval sec)##GPUParticleComp", &interval, 0.1f, 0.0f, 60.0f, "%.1f 秒 (0で単発のみ)")) {
            SetBurstInterval(interval);
        }
        int burstCount = static_cast<int>(burstCount_);
        if (ImGui::DragInt("バースト数 (Burst Count)##GPUParticleComp", &burstCount, 5, 1, 10000)) {
            SetBurstCount(static_cast<uint32_t>(burstCount));
        }
    }

    // 5. 時間（寿命）設定
    float lifeRange[2] = { minLifeTime_, maxLifeTime_ };
    if (ImGui::DragFloat2("寿命 (LifeTime Min/Max)##GPUParticleComp", lifeRange, 0.05f, 0.05f, 20.0f, "%.2f 秒")) {
        SetMinLifeTime(lifeRange[0]);
        SetMaxLifeTime(lifeRange[1]);
    }

    // 6. 重力設定
    float g = gravity_;
    if (ImGui::DragFloat("重力 (Gravity)##GPUParticleComp", &g, 0.1f, -50.0f, 50.0f, "%.2f")) {
        SetGravity(g);
    }

    // 7. カラー設定
    float sCol[4] = { startColor_.x, startColor_.y, startColor_.z, startColor_.w };
    if (ImGui::ColorEdit4("開始色 (Start Color)##GPUParticleComp", sCol)) {
        SetStartColor({ sCol[0], sCol[1], sCol[2], sCol[3] });
    }
    float eCol[4] = { endColor_.x, endColor_.y, endColor_.z, endColor_.w };
    if (ImGui::ColorEdit4("終了色 (End Color)##GPUParticleComp", eCol)) {
        SetEndColor({ eCol[0], eCol[1], eCol[2], eCol[3] });
    }

    // 8. 文字花火設定 (TextFirework または Fireworks 選択時)
    if (presetName_ == "TextFirework" || presetName_ == "Fireworks") {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "【文字花火設定】");
        char textBuf[kNameBufferSize]{};
        strncpy_s(textBuf, fireworkText_.c_str(), sizeof(textBuf) - 1);
        if (ImGui::InputText("浮き上がらせる文字##GPUParticleComp", textBuf, sizeof(textBuf))) {
            fireworkText_ = textBuf;
        }
        ImGui::SameLine();
        if (ImGui::Button("「祝」")) fireworkText_ = "祝";
        ImGui::SameLine();
        if (ImGui::Button("「WIN」")) fireworkText_ = "WIN";
        ImGui::SameLine();
        if (ImGui::Button("「花火」")) fireworkText_ = "花火";

        float height = launchHeight_;
        if (ImGui::DragFloat("打ち上げ高度##GPUParticleComp", &height, 0.5f, 1.0f, 100.0f, "%.1f m")) {
            launchHeight_ = height;
        }
        float tScale = textScale_;
        if (ImGui::DragFloat("文字サイズ##GPUParticleComp", &tScale, 0.5f, 1.0f, 50.0f, "%.1f m")) {
            textScale_ = tScale;
        }

        // 最新の点群をサンプリング
        const auto& currentPts = textSampler_.ExtractPoints(fireworkText_);
        if (particleObject_) {
            particleObject_->SetShapePoints(currentPts);
            particleObject_->SetShapeType(1);
            particleObject_->SetBurstSpeed(textScale_);
            float autoScale = 0.22f;



            if (currentPts.size() > 600) {
                autoScale = 0.12f;
            } else if (currentPts.size() > 300) {
                autoScale = 0.16f;
            }
            particleObject_->GetSetting().startScale = { autoScale, autoScale, autoScale };
            particleObject_->GetSetting().endScale = { autoScale * 0.15f, autoScale * 0.15f, autoScale * 0.15f };

        }
        ImGui::Text("文字ドット点群数: %zu 点", currentPts.size());

        if (ImGui::Button("[打ち上げテスト (Launch!)]##GPUParticleComp")) {
            fireworkPhase_ = FireworkPhase::Launching;
            launchProgress_ = 0.0f;
            currentLaunchOffsetY_ = 0.0f;
            burstTimer_ = 0.0f;
            if (particleObject_) {
                particleObject_->Reset();
            }
        }
    }

    // 9. 再生・停止・バーストアクションボタン
    ImGui::Separator();
    if (ImGui::Button("Play (再生/再開)##GPUParticleComp")) {
        Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop (停止)##GPUParticleComp")) {
        Stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("Emit Burst (即時放出)##GPUParticleComp")) {
        EmitBurst(burstCount_);
    }

    // 10. プリセットJSONとして保存
    ImGui::Separator();
    static char savePresetBuf[kNameBufferSize] = "CustomEffect";
    ImGui::InputText("保存名##GPUParticleComp", savePresetBuf, sizeof(savePresetBuf));
    ImGui::SameLine();
    if (ImGui::Button("JSON保存##GPUParticleComp")) {
        GPUParticleSetting s;
        s.name = savePresetBuf;
        s.textureKey = textureKey_;
        s.blendMode = static_cast<int>(blendMode_);
        s.emitterPosition = offset_;
        s.startScale = particleObject_ ? particleObject_->GetSetting().startScale : Vector3{ 0.35f, 0.35f, 0.35f };
        s.endScale = particleObject_ ? particleObject_->GetSetting().endScale : Vector3{ 0.05f, 0.05f, 0.05f };
        s.startColor = startColor_;
        s.endColor = endColor_;
        s.minVelocity = particleObject_ ? particleObject_->GetSetting().minVelocity : Vector3{ -2.5f, 1.0f, -2.5f };
        s.maxVelocity = particleObject_ ? particleObject_->GetSetting().maxVelocity : Vector3{ 2.5f, 6.0f, 2.5f };
        s.minLifeTime = minLifeTime_;
        s.maxLifeTime = maxLifeTime_;
        s.gravity = gravity_;
        s.killAtY = particleObject_ ? particleObject_->GetSetting().killAtY : -999.0f;
        s.spawnRatePerSec = isLoop_ ? spawnRatePerSec_ : 0.0f;
        s.isAutoSpawning = isLoop_;
        s.burstCount = burstCount_;

        std::string path = GPUParticlePresets::kParticleDirectory + "/" + savePresetBuf + GPUParticlePresets::kFileExtension;
        GPUParticlePresets::SaveToJson(s, path);
    }

    // 11. 内部詳細設定
    if (particleObject_) {
        if (ImGui::TreeNode("詳細シミュレーションパラメータ##GPUParticleDetails")) {
            particleObject_->ImGuiControl("Setting");
            ImGui::TreePop();
        }
    }
#endif
}
