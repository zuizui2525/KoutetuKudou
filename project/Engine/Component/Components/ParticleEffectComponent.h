#pragma once
#include "Engine/Component/IComponent.h"
#include "Engine/Math/MathStructs.h"
#include <string>

/// <summary>
/// エフェクト発生・制御を担うコンポーネント (UnityライクなParticleSystem/ParticleEffect)
/// </summary>
class ParticleEffectComponent : public IComponent {
public:
    ParticleEffectComponent() = default;
    ~ParticleEffectComponent() override = default;

    void Initialize() override {}
    void Update() override;
    void DrawInspector() override;

    std::string GetComponentTypeName() const override { return "ParticleEffect"; }

    // ==========================================
    // 再生制御
    // ==========================================
    void Play();
    void Stop();

    // ==========================================
    // パラメータ操作
    // ==========================================
    const std::string& GetEffectName() const { return effectName_; }
    void SetEffectName(const std::string& name) { effectName_ = name; }

    const Vector3& GetOffset() const { return offset_; }
    void SetOffset(const Vector3& offset) { offset_ = offset; }

    bool IsLoop() const { return isLoop_; }
    void SetLoop(bool loop) { isLoop_ = loop; }

    bool IsAutoPlay() const { return autoPlay_; }
    void SetAutoPlay(bool autoPlay) { autoPlay_ = autoPlay; }

private:
    std::string effectName_ = "Hit";
    Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
    bool isLoop_ = false;
    bool autoPlay_ = false;
    bool hasAutoPlayed_ = false;
};
