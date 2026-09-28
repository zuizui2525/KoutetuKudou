#pragma once
#include "EffectSetting.h"

// 水面へ落ちるメイン水滴の設定
struct WaterDropEffectSetting : public EffectSetting {
    WaterDropEffectSetting() {
        name = "WaterDrop";
        textureName = "white";
        meshType = "cube";

        emitCountMin = 1;
        emitCountMax = 1;
        lifeTimeMin = 2.0f; // 地面に着くまで消えないように十分長くする
        lifeTimeMax = 2.5f;

        // もっと広範囲に降らせる
        spawnAreaMin = { -30.0f, 0.0f, -30.0f };
        spawnAreaMax = { 30.0f, 0.0f, 30.0f };

        positionOffset = { 0.0f, 40.0f, 0.0f }; // もっと高いところから
        velocityMin = { 0.0f, -60.0f, 0.0f }; // もっと速く
        velocityMax = { 0.0f, -60.0f, 0.0f };

        scaleMin = { 0.1f, 0.1f, 0.1f };
        scaleMax = { 0.15f, 0.15f, 0.15f };

        colorStartMin = { 0.9f, 0.95f, 1.0f, 1.0f };
        colorStartMax = { 1.0f, 1.0f, 1.0f, 1.0f };

        isBillboard = false;
        isVelocityAligned = true; 
        velocityAlignmentScale = 0.03f; // 線をもっと短く

        isEmitter = true;         
        emitFrequency = 0.08f;   // 雨の密度（負荷軽減のため頻度を低下）

        killAtY = 0.05f; // 地面（Y=0付近）で確実に消滅させる

        // 落下中にさらに小さな水滴を落とす
        trailEffectName = "DropletTrail";
        trailFrequency = 0.4f; // トレイル生成間隔を広げて負荷を低減

        // 地面に落ちた瞬間にメインの波紋を発生させる
        onDeathEffectName = "RippleSplash,RippleRing,RippleSplashParticles";
    }
};

// メイン水滴からこぼれ落ちる小水滴
struct DropletTrailEffectSetting : public EffectSetting {
    DropletTrailEffectSetting() {
        name = "DropletTrail";
        textureName = "white";
        meshType = "cube";

        emitCountMin = 1;
        emitCountMax = 1;
        lifeTimeMin = 1.0f;
        lifeTimeMax = 1.5f;

        // 親（メイン水滴）の速度を少し引き継ぎつつ、バラつかせる
        velocityMin = { -2.0f, -10.0f, -2.0f };
        velocityMax = { 2.0f, 0.0f, 2.0f };

        scaleMin = { 0.05f, 0.05f, 0.05f };
        scaleMax = { 0.1f, 0.1f, 0.1f };

        isVelocityAligned = true;
        velocityAlignmentScale = 0.03f;
        
        killAtY = 0.05f; // 小さな粒も地面で消えるようにする

        // 消滅時（着水時）に小さな波紋を出す
        onDeathEffectName = "SmallRipple";
    }
};

// 小水滴用の小さな波紋
struct SmallRippleEffectSetting : public EffectSetting {
    SmallRippleEffectSetting() {
        name = "SmallRipple";
        textureName = "circle";
        meshType = "flat_ring";

        emitCountMin = 1;
        emitCountMax = 1;
        lifeTimeMin = 1.4f;
        lifeTimeMax = 1.6f;

        rotationMin = { 1.57079f, 0.0f, 0.0f };
        rotationMax = { 1.57079f, 0.0f, 0.0f };

        scaleMin = { 0.1f, 0.1f, 0.1f };
        scaleMax = { 0.1f, 0.1f, 0.1f };
        scaleEndMin = { 0.8f, 0.8f, 1.0f }; // さらに小さく
        scaleEndMax = { 1.5f, 1.5f, 1.0f };

        colorStartMin = { 0.8f, 0.9f, 1.0f, 0.2f }; // 透明度を下げる
        colorEndMin = { 0.5f, 0.8f, 1.0f, 0.0f };

        isBillboard = false;
        useEaseOutScale = true;
        useEaseOutAlpha = true;
        isUniformScaleXZ = true;
    }
};

// 波紋の「広がる輪」の設定
struct RippleRingEffectSetting : public EffectSetting {
    RippleRingEffectSetting() {
        name = "RippleRing";
        textureName = "circle";
        meshType = "flat_ring";

        emitCountMin = 1;
        emitCountMax = 1;
        lifeTimeMin = 1.6f; 
        lifeTimeMax = 1.8f;

        rotationMin = { 1.57079f, 0.0f, 0.0f };
        rotationMax = { 1.57079f, 0.0f, 0.0f };

        scaleMin = { 0.1f, 0.1f, 0.1f };
        scaleMax = { 0.1f, 0.1f, 0.1f };
        scaleEndMin = { 4.0f, 4.0f, 1.0f }; // さらに小さく
        scaleEndMax = { 6.0f, 6.0f, 1.0f };

        colorStartMin = { 1.0f, 1.0f, 1.0f, 0.4f }; // 透明度を下げる
        colorStartMax = { 1.0f, 1.0f, 1.0f, 0.5f };
        colorEndMin = { 0.5f, 0.8f, 1.0f, 0.0f };
        colorEndMax = { 0.5f, 0.8f, 1.0f, 0.0f };

        isBillboard = false;
        useEaseOutScale = true; 
        useEaseOutAlpha = true;
        isUniformScaleXZ = true;
    }
};

// 波紋の「立ち上がる飛沫」の設定
struct RippleSplashEffectSetting : public EffectSetting {
    RippleSplashEffectSetting() {
        name = "RippleSplash";
        textureName = "gradationLine";
        meshType = "cylinder";

        emitCountMin = 1;
        emitCountMax = 1;
        lifeTimeMin = 2.8f; 
        lifeTimeMax = 3.0f;

        rotationVelocityMin = { 0.0f, 0.2f, 0.0f }; 
        rotationVelocityMax = { 0.0f, 0.4f, 0.0f };

        scaleMin = { 0.0f, 0.0f, 0.0f };
        scaleMax = { 0.0f, 0.0f, 0.0f };
        scaleEndMin = { 4.0f, 1.0f, 4.0f }; // 半径と高さを半分に
        scaleEndMax = { 6.0f, 1.5f, 6.0f };

        colorStartMin = { 1.0f, 1.0f, 1.0f, 0.2f }; // 透明度を下げる
        colorStartMax = { 1.0f, 1.0f, 1.0f, 0.3f };
        colorEndMin = { 0.2f, 0.6f, 1.0f, 0.0f };
        colorEndMax = { 0.4f, 0.7f, 1.0f, 0.0f };

        isBillboard = false;
        useSineScaleY = true;
        useSineScaleXZ = true;
        isUniformScaleXZ = true;
    }
};

// 着水時に飛び散る細かな水しぶき粒子の設定
struct RippleSplashParticlesEffectSetting : public EffectSetting {
    RippleSplashParticlesEffectSetting() {
        name = "RippleSplashParticles";
        textureName = "white";
        meshType = "cube";

        emitCountMin = 3; // パーティクル数を減らして負荷を低減
        emitCountMax = 5;
        lifeTimeMin = 0.3f;
        lifeTimeMax = 0.6f;

        velocityMin = { 4.0f, 4.0f, 4.0f }; 
        velocityMax = { 8.0f, 8.0f, 8.0f };
        acceleration = { 0.0f, -30.0f, 0.0f }; 

        scaleMin = { 0.05f, 0.05f, 0.05f };
        scaleMax = { 0.1f, 0.1f, 0.1f };
        scaleEndMin = { 0.0f, 0.0f, 0.0f };
        scaleEndMax = { 0.0f, 0.0f, 0.0f };

        colorStartMin = { 0.9f, 0.95f, 1.0f, 1.0f };
        colorStartMax = { 1.0f, 1.0f, 1.0f, 1.0f };
        colorEndMin = { 0.5f, 0.8f, 1.0f, 0.0f };
        colorEndMax = { 0.5f, 0.8f, 1.0f, 0.0f };

        isBillboard = false;
        isSpherical = true;
    }
};
