#pragma once
#include "App/Scene/Core/IScene.h"
#include <memory>

class PostProcess;


#include "Engine/Input/Input.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/Triangle/TriangleObject.h"
#include "Engine/Graphics/Objects/3d/Square/SquareObject.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/TriangularPyramid/TriangularPyramidObject.h"
#include "Engine/Graphics/Objects/3d/Pyramid/PyramidObject.h"
#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"
#include "Engine/Graphics/Objects/3d/HemiSphere/HemiSphereObject.h"
#include "Engine/Graphics/Objects/3d/Cone/ConeObject.h"
#include "Engine/Graphics/Objects/3d/Cylinder/CylinderObject.h"
#include "Engine/Graphics/Objects/3d/Ring/RingObject.h"
#include "Engine/Graphics/Objects/3d/Model/ModelObject.h"
#include "Engine/Graphics/Objects/3d/Skybox/Skybox.h"
#include "Engine/Graphics/Objects/Light/Manager/LightManager.h"
#include "Engine/Graphics/Objects/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Objects/Camera/Debug/DebugCamera.h"
#include "Engine/Graphics/Objects/2d/Sprite/SpriteObject.h"
#include "Engine/Graphics/Objects/2d/Triangle/Triangle2DObject.h"
#include "Engine/Graphics/Objects/2d/Circle/Circle2DObject.h"
#include "Engine/Graphics/Objects/2d/Ring/Ring2DObject.h"
#include "Engine/Graphics/Objects/2d/Line/Line2DObject.h"

class TitleScene : public IScene {
public:
    void Initialize() override;
    void ImGuiControl() override;
    void Update() override;
    void Draw() override;
    void Draw2D() override;

private:
    // マネージャへのポインタ
    Input* input_ = nullptr;
    CameraManager* cameraMgr_ = nullptr;
    LightManager* lightMgr_ = nullptr;
    PostProcess* postProcess_ = nullptr;

    
    // オブジェクト
    std::shared_ptr<BaseCamera> mainCamera_;
    std::shared_ptr<DebugCamera> debugCamera_;

    std::unique_ptr<DirectionalLightObject> dirLight_;

    std::unique_ptr<LineObject> line_;
    std::unique_ptr<TriangleObject> triangle_;
    std::unique_ptr<SquareObject> square_;
    std::unique_ptr<CubeObject> cube_;
    std::unique_ptr<TriangularPyramidObject> triangularPyramid_;
    std::unique_ptr<PyramidObject> pyramid_;
    std::unique_ptr<SphereObject> sphere_;
    std::unique_ptr<HemisphereObject> hemisphere_;
    std::unique_ptr<ConeObject> cone_;
    std::unique_ptr<CylinderObject> cylinder_;
    std::unique_ptr<RingObject> ring_;
    std::unique_ptr<ModelObject> bunny_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<SpriteObject> testSprite_; // 2D矩形スプライト
    std::unique_ptr<Triangle2DObject> testTriangle2D_; // 2D三角形
    std::unique_ptr<Circle2DObject> testCircle2D_;     // 2D円
    std::unique_ptr<Ring2DObject> testRing2D_;         // 2Dリング
    std::unique_ptr<Line2DObject> testLine2D_;         // 2Dライン
};
