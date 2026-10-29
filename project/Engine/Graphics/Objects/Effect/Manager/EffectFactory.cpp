#include "EffectFactory.h"
#include "../Settings/DefaultEffectSetting.h"
#include "../Settings/HitEffectSetting.h"
#include "../Settings/SlashEffectSetting.h"
#include "../Settings/FireEffectSetting.h"
#include "../Settings/YellowFireEffectSetting.h"
#include "../Settings/PurpleFireEffectSetting.h"
#include "../Settings/ExplosionEffectSetting.h"
#include "../Settings/DragonBreathEffectSetting.h"
#include "../Settings/FireworksBurstEffectSetting.h"
#include "../Settings/FireworksSparkleEffectSetting.h"
#include "../Settings/FireworksFlameEffectSetting.h"
#include "../Settings/FireworksAscendEffectSetting.h"
#include "../Settings/FireworksSetEffectSetting.h"
#include "../Settings/RingAuraEffectSetting.h"
#include "../Settings/RingSlashEffectSetting.h"
#include "../Settings/CylinderAuraEffectSetting.h"
#include "../Settings/RippleEffectSetting.h"
#include "../Settings/BulletTrailEffectSetting.h"

EffectFactory* EffectFactory::GetInstance() {
    static EffectFactory instance;
    return &instance;
}

void EffectFactory::RegisterAllEffects() {
    auto effectMgr = EffectManager::GetInstance();

    // 各クラスのインスタンスを生成して登録
    effectMgr->RegisterEffect(DefaultEffectSetting());
    effectMgr->RegisterEffect(HitEffectSetting());
    effectMgr->RegisterEffect(SlashEffectSetting());
    effectMgr->RegisterEffect(FireEffectSetting());
    effectMgr->RegisterEffect(YellowFireEffectSetting());
    effectMgr->RegisterEffect(PurpleFireEffectSetting());
    effectMgr->RegisterEffect(ExplosionEffectSetting());
    effectMgr->RegisterEffect(DragonBreathEffectSetting());
    effectMgr->RegisterEffect(FireworksBurstEffectSetting());
    effectMgr->RegisterEffect(FireworksSparkleEffectSetting());
    effectMgr->RegisterEffect(FireworksFlameEffectSetting());
    effectMgr->RegisterEffect(FireworksAscendEffectSetting());
    effectMgr->RegisterEffect(FireworksSetEffectSetting());
    effectMgr->RegisterEffect(RingAuraEffectSetting());
    effectMgr->RegisterEffect(RingSlashEffectSetting());
    effectMgr->RegisterEffect(CylinderAuraEffectSetting());
    effectMgr->RegisterEffect(RippleRingEffectSetting());
    effectMgr->RegisterEffect(RippleSplashEffectSetting());
    effectMgr->RegisterEffect(RippleSplashParticlesEffectSetting());
    effectMgr->RegisterEffect(DropletTrailEffectSetting());
    effectMgr->RegisterEffect(SmallRippleEffectSetting());
    effectMgr->RegisterEffect(WaterDropEffectSetting());
    effectMgr->RegisterEffect(BulletTrailEffectSetting());
}

