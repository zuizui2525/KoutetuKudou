#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>
#include <memory>
#include <vector>

#include "Engine/Graphics/Objects/Effect/GPU/GPUParticleObject.h"

/// <summary>
/// 各GameObjectに完全独立したGPUパーティクルエミッターを付与するコンポーネント。
/// オブジェクトプールパターンにより、毎フレームのVRAM再割り当てやGPUストールをゼロに抑えて高速動作します。
/// </summary>
class GPUParticleComponent : public IComponent {
public:
    // マジックナンバー排除用定数
    static constexpr uint32_t kDefaultComponentMaxParticles = 512;
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

    // オブジェクトプール
    static inline std::vector<std::unique_ptr<GPUParticleObject>> sParticlePool_;
};
