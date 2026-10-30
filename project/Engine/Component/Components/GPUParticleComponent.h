#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>
#include <memory>
#include <vector>

#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"
#include "Engine/Graphics/Text/TextShapeSampler.h"

/// <summary>
/// 各GameObjectに完全独立したGPUパーティクルエミッターを付与するコンポーネント。
/// オブジェクトプールパターンにより、毎フレームのVRAM再割り当てやGPUストールをゼロに抑えて高速動作します。
/// </summary>
class GPUParticleComponent : public IComponent {
public:
    // マジックナンバー排除用定数
    static constexpr uint32_t kDefaultComponentMaxParticles = 2048;
    static constexpr float kDefaultLaunchDuration = 0.75f;
    static constexpr uint32_t kFireworkBurstCount = 1024;
    static constexpr float kDefaultSpawnRate = 50.0f;
    static constexpr float kDefaultDeltaTime = 1.0f / 60.0f;

    GPUParticleComponent();
    ~GPUParticleComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "GPUParticle"; }

    // プリセット適用
    void ApplyPreset(const std::string& presetName);

    // バースト放出
    void EmitBurst(uint32_t count);

    // 再生・停止制御（旧ParticleEffect互換API）
    void Play();
    void Stop();

    // エフェクト名設定（旧ParticleEffect互換エイリアス）
    const std::string& GetEffectName() const { return presetName_; }
    void SetEffectName(const std::string& name) { SetPresetName(name); }
    bool IsAutoPlay() const { return IsLoop(); }
    void SetAutoPlay(bool autoPlay) { SetLoop(autoPlay); }

    // Getter / Setter
    const std::string& GetPresetName() const { return presetName_; }
    void SetPresetName(const std::string& name);

    const std::string& GetTextureKey() const { return textureKey_; }
    void SetTextureKey(const std::string& key) { textureKey_ = key; }

    const Vector3& GetOffset() const { return offset_; }
    void SetOffset(const Vector3& offset) { offset_ = offset; }

    bool IsAutoSpawning() const { return isAutoSpawning_; }
    void SetAutoSpawning(bool active);

    float GetSpawnRate() const { return spawnRatePerSec_; }
    void SetSpawnRate(float rate);

    ParticleBlendMode GetBlendMode() const { return blendMode_; }
    void SetBlendMode(ParticleBlendMode mode);

    // ループ・時間・間隔制御
    bool IsLoop() const { return isLoop_; }
    void SetLoop(bool loop);

    float GetBurstInterval() const { return burstInterval_; }
    void SetBurstInterval(float interval) { burstInterval_ = interval; }

    uint32_t GetBurstCount() const { return burstCount_; }
    void SetBurstCount(uint32_t count) { burstCount_ = count; }

    float GetMinLifeTime() const { return minLifeTime_; }
    void SetMinLifeTime(float life);

    float GetMaxLifeTime() const { return maxLifeTime_; }
    void SetMaxLifeTime(float life);

    float GetGravity() const { return gravity_; }
    void SetGravity(float g);

    const Vector4& GetStartColor() const { return startColor_; }
    void SetStartColor(const Vector4& color);

    const Vector4& GetEndColor() const { return endColor_; }
    void SetEndColor(const Vector4& color);

    // 文字花火専用パラメータ
    const std::string& GetFireworkText() const { return fireworkText_; }
    void SetFireworkText(const std::string& text) { fireworkText_ = text; }

    float GetLaunchHeight() const { return launchHeight_; }
    void SetLaunchHeight(float height) { launchHeight_ = height; }

    float GetTextScale() const { return textScale_; }
    void SetTextScale(float scale) { textScale_ = scale; }

    GPUParticleObject* GetParticleObject() { return particleObject_.get(); }

    // プール全解放（アプリケーション終了時等に安全にクリーンアップ）
    static void ClearPool();

private:
    void AcquireParticleObject();
    void ReleaseParticleObject();

    std::unique_ptr<GPUParticleObject> particleObject_;
    std::string presetName_ = "BulletTrail";
    std::string textureKey_ = "circle";
    ParticleBlendMode blendMode_ = ParticleBlendMode::Add;
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
    float spawnRatePerSec_ = kDefaultSpawnRate;
    bool isAutoSpawning_ = true;
    bool isLoop_ = true;
    float burstInterval_ = 2.0f;
    float burstTimer_ = 0.0f;
    uint32_t burstCount_ = 30;

    float minLifeTime_ = 0.6f;
    float maxLifeTime_ = 1.0f;
    float gravity_ = 2.2f;
    Vector4 startColor_ = { 1.0f, 0.8f, 0.2f, 1.0f };
    Vector4 endColor_ = { 1.0f, 0.1f, 0.0f, 0.0f };

    std::string fireworkText_ = "祝";
    float launchHeight_ = 30.0f;
    float textScale_ = 15.0f;

    enum class FireworkPhase {
        Idle,
        Launching,
        Exploded
    };

    FireworkPhase fireworkPhase_ = FireworkPhase::Idle;
    float launchProgress_ = 0.0f;
    float currentLaunchOffsetY_ = 0.0f;
    TextShapeSampler textSampler_;

    // オブジェクトプール
    static inline std::vector<std::unique_ptr<GPUParticleObject>> sParticlePool_;
};
