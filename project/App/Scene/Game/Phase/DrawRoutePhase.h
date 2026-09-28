#pragma once
#include "App/Scene/Game/Phase/IGamePhase.h"
#include <functional>
#include "Engine/Math/MathStructs.h"

class Input;
class CameraManager;
class Route;
class Stage;
class Minimap;
class DrawRouteCamera;
class SphereObject;
class StageEditor;

/**
 * @brief ルートの手書き描画・ズームビューを管理するフェーズクラス
 */
class DrawRoutePhase : public IGamePhase {
public:
    DrawRoutePhase(
        Input* input,
        CameraManager* cameraMgr,
        Route* route,
        Stage* stage,
        Minimap* minimap,
        DrawRouteCamera* drawRouteCamera,
        SphereObject* cursorIndicatorZoom,
        StageEditor* stageEditor,
        std::function<void()> onStartGame
    );
    ~DrawRoutePhase() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void ImGuiControl() override;

    void SetShowRouteEditorPtr(bool* showRouteEditor) { showRouteEditor_ = showRouteEditor; }

private:
    Input* input_ = nullptr;
    CameraManager* cameraMgr_ = nullptr;
    Route* route_ = nullptr;
    Stage* stage_ = nullptr;
    Minimap* minimap_ = nullptr;
    DrawRouteCamera* drawRouteCamera_ = nullptr;
    SphereObject* cursorIndicatorZoom_ = nullptr;
    StageEditor* stageEditor_ = nullptr;
    std::function<void()> onStartGame_;
    bool* showRouteEditor_ = nullptr;
};
