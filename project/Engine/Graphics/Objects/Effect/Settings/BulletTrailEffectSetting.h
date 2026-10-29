#pragma once
#include "EffectSetting.h"

/// <summary>
/// 弾丸の背後に連なる球体軌跡（弾道トレイル）エフェクト設定
/// 直線上にその場に留まり、急速に縮小・フェードアウトすることで美麗な弾道ラインを形成します。
/// </summary>
class BulletTrailEffectSetting : public EffectSetting {
public:
    BulletTrailEffectSetting() {
        // 定数定義 (マジックナンバー排除)
        constexpr float kEmitFrequency = 0.02f;
        constexpr uint32_t kEmitCountMin = 1;
        constexpr uint32_t kEmitCountMax = 1;
        constexpr uint32_t kMaxParticles = 100;
        constexpr float kLifeTimeMin = 0.04f; // 2〜3フレームでスッと消滅
        constexpr float kLifeTimeMax = 0.06f;
        constexpr float kScaleMinVal = 0.12f; // 弾丸本体の幅に合わせた控えめな極小サイズ
        constexpr float kScaleMaxVal = 0.15f;
        constexpr float kScaleEndVal = 0.01f;

        name = "BulletTrail";
        textureName = "circle";
        isBillboard = true;
        isEmitter = false;
        emitFrequency = kEmitFrequency;
        emitCountMin = kEmitCountMin;
        emitCountMax = kEmitCountMax;
        maxParticles = kMaxParticles;
        lifeTimeMin = kLifeTimeMin;
        lifeTimeMax = kLifeTimeMax;

        // 拡散初速・ブレをゼロにし、弾丸の通過位置にピタッと留まらせる
        spawnAreaMin = { 0.0f, 0.0f, 0.0f };
        spawnAreaMax = { 0.0f, 0.0f, 0.0f };
        velocityMin = { 0.0f, 0.0f, 0.0f };
        velocityMax = { 0.0f, 0.0f, 0.0f };
        acceleration = { 0.0f, 0.0f, 0.0f };

        // スケール変化（球体がシュッと縮小）
        scaleMin = { kScaleMinVal, kScaleMinVal, kScaleMinVal };
        scaleMax = { kScaleMaxVal, kScaleMaxVal, kScaleMaxVal };
        scaleEndMin = { kScaleEndVal, kScaleEndVal, kScaleEndVal };
        scaleEndMax = { kScaleEndVal, kScaleEndVal, kScaleEndVal };

        // 発光カラー（中心が眩しい高輝度イエロー・ホワイト、消滅時にオレンジフェード）
        colorStartMin = { 1.0f, 0.95f, 0.50f, 0.90f };
        colorStartMax = { 1.0f, 1.0f, 0.80f, 1.0f };
        colorEndMin = { 1.0f, 0.55f, 0.10f, 0.0f };
        colorEndMax = { 1.0f, 0.70f, 0.20f, 0.0f };

        // フェードアウトとイージング
        useEaseOutAlpha = true;
        useEaseInScale = true;
    }
};
