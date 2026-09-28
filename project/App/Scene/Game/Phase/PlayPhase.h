#pragma once
#include "App/Scene/Game/Phase/IGamePhase.h"
#include <functional>
#include <string>

class Input;
class CameraManager;
class Route;
class Stage;
class Player;
class EnemyManager;
class PlayCamera;
class Reticle;

/**
 * @brief ルート自動走行および戦闘を管理するプレイフェーズクラス
 */
class PlayPhase : public IGamePhase {
public:
    PlayPhase(
        Input* input,
        CameraManager* cameraMgr,
        Route* route,
        Stage* stage,
        Player* player,
        EnemyManager* enemyManager,
        PlayCamera* playCamera,
        Reticle* reticle,
        float& ioCurrentDistance,
        std::function<void()> onAreaCleared
    );
    ~PlayPhase() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void ImGuiControl() override;

private:
    // 定数
    static inline const std::string kRainEffectName = "WaterDrop"; // 雨のエフェクト名
    static inline const std::string kClearSceneName = "Clear";     // クリアシーン名
    static inline const int kMaxAreaIndex = 3;                     // 最大エリアインデックス

private:
    Input* input_ = nullptr;
    CameraManager* cameraMgr_ = nullptr;
    Route* route_ = nullptr;
    Stage* stage_ = nullptr;
    Player* player_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    PlayCamera* playCamera_ = nullptr;
    Reticle* reticle_ = nullptr;
    
    float& currentDistance_; // GameSceneの現在距離の参照
    std::function<void()> onAreaCleared_;
};
