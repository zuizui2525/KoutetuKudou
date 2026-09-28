#pragma once
#include <memory>
#include "Engine/Graphics/Objects/Light/Directional/DirectionalLight.h"

class LightManager;

/**
 * @brief ゲーム用ライト（平行光源）管理クラス
 */
class GameLight {
public:
    GameLight();
    ~GameLight();

    // 初期化処理
    void Initialize(LightManager* lightMgr);
    
    // 更新処理
    void Update();

private:
    std::unique_ptr<DirectionalLightObject> dirLight_;
    LightManager* lightMgr_ = nullptr;
};
