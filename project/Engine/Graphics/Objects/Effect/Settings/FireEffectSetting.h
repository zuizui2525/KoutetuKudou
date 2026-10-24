#pragma once
#include "EffectSetting.h"

class FireEffectSetting : public EffectSetting {
public:
    FireEffectSetting() {
        // 定数定義 (マジックナンバー排除)
        constexpr float kEmitFrequency = 0.02f;
        constexpr uint32_t kEmitCountMin = 1;
        constexpr uint32_t kEmitCountMax = 2;
        constexpr float kLifeTimeMin = 0.18f;
        constexpr float kLifeTimeMax = 0.32f;
        constexpr float kSpawnSpread = 0.06f;
        constexpr float kVelSpread = 0.35f;
        constexpr float kScaleMinVal = 0.40f;
        constexpr float kScaleMaxVal = 0.60f;
        constexpr float kScaleEndMinVal = 0.08f;
        constexpr float kScaleEndMaxVal = 0.15f;

        name = "Fire";
        textureName = "circle";
        isBillboard = true;
        isEmitter = true;
        emitFrequency = kEmitFrequency;
        emitCountMin = kEmitCountMin;
        emitCountMax = kEmitCountMax;
        lifeTimeMin = kLifeTimeMin;
        lifeTimeMax = kLifeTimeMax;
        spawnAreaMin = { -kSpawnSpread, -kSpawnSpread, -kSpawnSpread };
        spawnAreaMax = {  kSpawnSpread,  kSpawnSpread,  kSpawnSpread };
        velocityMin = { -kVelSpread, -kVelSpread, -kVelSpread };
        velocityMax = {  kVelSpread,  kVelSpread,  kVelSpread };
        scaleMin = { kScaleMinVal, kScaleMinVal, kScaleMinVal };
        scaleMax = { kScaleMaxVal, kScaleMaxVal, kScaleMaxVal };
        scaleEndMin = { kScaleEndMinVal, kScaleEndMinVal, kScaleEndMinVal };
        scaleEndMax = { kScaleEndMaxVal, kScaleEndMaxVal, kScaleEndMaxVal };
        colorStartMin = { 1.0f, 0.60f, 0.10f, 1.0f }; // 高熱の黄金オレンジ
        colorStartMax = { 1.0f, 0.95f, 0.30f, 1.0f }; // 鮮やかな白黄色
        colorEndMin = { 0.80f, 0.15f, 0.00f, 0.0f };   // 赤熱フェード
        colorEndMax = { 1.00f, 0.30f, 0.00f, 0.0f };
        useEaseOutAlpha = true;
    }
};
