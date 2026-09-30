#include "App/Scene/Title/TitleScene.h"
#include "Engine/Base/BaseResource.h"
#include "App/Scene/Core/SceneManager.h"
#include "Engine/Graphics/PostProcess/PostProcess.h"

namespace {
    // 2Dテスト用オブジェクト定数 (マジックナンバー排除)
    static inline const Vector3 kTestSpritePosition = { 100.0f, 150.0f, 0.0f };
    static constexpr float kTestSpriteWidth = 120.0f;
    static constexpr float kTestSpriteHeight = 120.0f;

    static inline const Vector3 kTestTrianglePosition = { 280.0f, 150.0f, 0.0f };
    static constexpr float kTestTriangleWidth = 120.0f;
    static constexpr float kTestTriangleHeight = 120.0f;
    static inline const Vector4 kTestTriangleColor = { 0.2f, 0.8f, 1.0f, 1.0f }; // シアン

    static inline const Vector3 kTestCirclePosition = { 460.0f, 150.0f, 0.0f };
    static constexpr float kTestCircleRadius = 60.0f;
    static inline const Vector4 kTestCircleColor = { 1.0f, 0.85f, 0.2f, 1.0f }; // イエロー

    static inline const Vector3 kTestRingPosition = { 640.0f, 150.0f, 0.0f };
    static constexpr float kTestRingOuterRadius = 60.0f;
    static constexpr float kTestRingInnerRadius = 40.0f;
    static inline const Vector4 kTestRingColor = { 1.0f, 0.3f, 0.6f, 1.0f }; // ピンク

    static inline const Vector2 kTestLineStart = { 100.0f, 320.0f };
    static inline const Vector2 kTestLineEnd = { 760.0f, 320.0f };
    static constexpr float kTestLineThickness = 6.0f;
    static inline const Vector4 kTestLineColor = { 0.4f, 1.0f, 0.4f, 1.0f }; // ライムグリーン

    static inline const std::string kDefaultSpriteTex = "white";
}


void TitleScene::Initialize() {
    // 0. ポストプロセスのポインタを取得してメンバ変数に保持し、初期状態でグレースケールを有効にする
    postProcess_ = SceneManager::GetInstance()->GetPostProcess();
    if (postProcess_) {
		postProcess_->SetDepthOutlineActive(true);
		postProcess_->SetGrayscaleActive(true);
		postProcess_->SetVignetteActive(true);
    }

    // 1. 各マネージャの取得
    cameraMgr_ = CameraResource::GetCameraManager();
    lightMgr_ = LightResource::GetLightManager();
    input_ = InputResource::GetInput();

    // 2. カメラの生成と登録
    mainCamera_ = std::make_shared<BaseCamera>();
    mainCamera_->Initialize();
    cameraMgr_->AddCamera("Main", mainCamera_);
    cameraMgr_->SetActiveCamera("Main");

    debugCamera_ = std::make_shared<DebugCamera>();
    debugCamera_->Initialize();
    cameraMgr_->AddCamera("Debug", debugCamera_);
    cameraMgr_->SetActiveCamera("Main");

    // 3. ライトの生成（ディレクショナルライト）
    dirLight_ = std::make_unique<DirectionalLightObject>();
    dirLight_->Initialize();
    lightMgr_->AddDirectionalLight(dirLight_.get());

    // 4. モデルの生成（ロード済み）
    line_ = std::make_unique<LineObject>();
    line_->Initialize();
    line_->SetStartPoint({3.0f, 0.0f, 0.0f});
    line_->SetEndPoint({ -3.0f, 2.0f, 0.0f });

    triangle_ = std::make_unique<TriangleObject>();
    triangle_->Initialize();
    triangle_->SetPosition({ -2.0f, 2.0f, 0.0f });

    square_ = std::make_unique<SquareObject>();
    square_->Initialize();
    square_->SetPosition({ -2.0f, 0.0f, 0.0f });

    cube_ = std::make_unique<CubeObject>();
    cube_->Initialize();
    cube_->SetPosition({ 2.0f, 2.0f, 0.0f });

    triangularPyramid_ = std::make_unique<TriangularPyramidObject>();
    triangularPyramid_->Initialize();
    triangularPyramid_->SetPosition({ 4.0f, -2.0f, 0.0f });

    pyramid_ = std::make_unique<PyramidObject>();
    pyramid_->Initialize();
    pyramid_->SetPosition({ 4.0f, 2.0f, 0.0f });

    sphere_ = std::make_unique<SphereObject>();
    sphere_->Initialize();
    sphere_->SetPosition({ 2.0f, 0.0f, 0.0f });

    hemisphere_ = std::make_unique<HemisphereObject>();
    hemisphere_->Initialize();
    hemisphere_->SetPosition({ -4.0f, 0.0f, 0.0f });

    cone_ = std::make_unique<ConeObject>();
    cone_->Initialize();
    cone_->SetPosition({ 4.0f, 0.0f, 0.0f });

    cylinder_ = std::make_unique<CylinderObject>();
    cylinder_->Initialize();
    cylinder_->SetPosition({ 2.0f, -2.0f, 0.0f });

    ring_ = std::make_unique<RingObject>();
    ring_->Initialize();
    ring_->SetPosition({ -2.0f, -2.0f, 0.0f });

    bunny_ = std::make_unique<ModelObject>();
    bunny_->Initialize();

    // 5. Skyboxの生成
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize();

    // 6. 2Dオブジェクトの生成と配置 (2D描画テスト用)
    testSprite_ = std::make_unique<SpriteObject>();
    testSprite_->Initialize();
    testSprite_->SetPosition(kTestSpritePosition);
    testSprite_->SetSize(kTestSpriteWidth, kTestSpriteHeight);

    testTriangle2D_ = std::make_unique<Triangle2DObject>();
    testTriangle2D_->Initialize();
    testTriangle2D_->SetPosition(kTestTrianglePosition);
    testTriangle2D_->SetSize(kTestTriangleWidth, kTestTriangleHeight);
    testTriangle2D_->GetMaterialData()->color = kTestTriangleColor;

    testCircle2D_ = std::make_unique<Circle2DObject>();
    testCircle2D_->Initialize();
    testCircle2D_->SetPosition(kTestCirclePosition);
    testCircle2D_->SetRadius(kTestCircleRadius);
    testCircle2D_->GetMaterialData()->color = kTestCircleColor;

    testRing2D_ = std::make_unique<Ring2DObject>();
    testRing2D_->Initialize();
    testRing2D_->SetPosition(kTestRingPosition);
    testRing2D_->SetRadii(kTestRingOuterRadius, kTestRingInnerRadius);
    testRing2D_->GetMaterialData()->color = kTestRingColor;

    testLine2D_ = std::make_unique<Line2DObject>();
    testLine2D_->Initialize();
    testLine2D_->SetPoints(kTestLineStart, kTestLineEnd);
    testLine2D_->SetThickness(kTestLineThickness);
    testLine2D_->GetMaterialData()->color = kTestLineColor;
}

void TitleScene::ImGuiControl() {
#ifdef _USEIMGUI
    cameraMgr_->ImGuiControl();

    // ポストプロセスのパラメータ調整用ImGuiコントロール
    if (postProcess_) {
        postProcess_->ImGuiControl();
    }
#endif
}

void TitleScene::Update() {
    // シーン切り替え
    if (input_->Trigger(DIK_SPACE)) {
        SceneManager::GetInstance()->ChangeScene("Game");
    }

#ifdef _USEIMGUI
    // モード切り替え（TABキー）
    if (input_->Trigger(DIK_TAB)) {
        bool isCurrentlyDebug = (cameraMgr_->GetActiveCamera() == debugCamera_.get());
        cameraMgr_->SetActiveCamera(isCurrentlyDebug ? "Main" : "Debug");
    }
#endif

    // ライトとオブジェクトの更新
    dirLight_->Update();
    line_->Update();
    triangle_->Update();
    square_->Update();
    cube_->Update();
    triangularPyramid_->Update();
    pyramid_->Update();
    sphere_->Update();
    hemisphere_->Update();
    cone_->Update();
    cylinder_->Update();
    ring_->Update();
    bunny_->Update();
    skybox_->Update();

    if (testSprite_) {
        testSprite_->Update();
    }
    if (testTriangle2D_) {
        testTriangle2D_->Update();
    }
    if (testCircle2D_) {
        testCircle2D_->Update();
    }
    if (testRing2D_) {
        testRing2D_->Update();
    }
    if (testLine2D_) {
        testLine2D_->Update();
    }

    // カメラの更新
    BaseCamera* active = cameraMgr_->GetActiveCamera();
    DebugCamera* dc = dynamic_cast<DebugCamera*>(active);

    if (dc) {
        dc->SetActive(true);
        dc->Update(input_);
    } else {
        debugCamera_->SetActive(false);
        active->Update();
    }
}

void TitleScene::Draw() {
    // Skyboxの描画（透過を含まない他のモデルより先、または後に描画）
    skybox_->Draw("forestTex");

    // 線の描画
    line_->Draw();

    // 三角形の描画
    triangle_->Draw("white", "forestTex");

    // 四角形の描画
    square_->Draw("white", "forestTex");

    // 立方体の描画
    cube_->Draw("white", "forestTex");

    // 三角錐の描画
    triangularPyramid_->Draw("white", "forestTex");

    // 四角錐の描画
    pyramid_->Draw("white", "forestTex");

    // 球体の描画
    sphere_->Draw("white", "forestTex");

    // 半球体の描画
    hemisphere_->Draw("white", "forestTex");

    // 円錐の描画
    cone_->Draw("white", "forestTex");

    // 円柱の描画
    cylinder_->Draw("white", "forestTex");

    // リングの描画
    ring_->Draw("white", "forestTex");

    // バニーの描画（第3引数に環境マップのキーを指定）
    bunny_->Draw("bunny", "white", "forestTex");
}

void TitleScene::Draw2D() {
    // 2Dオブジェクト群の描画 (ポストプロセス完了後の最終テクスチャに対するオーバーレイ描画)
    if (testSprite_) {
        testSprite_->Draw(kDefaultSpriteTex);
    }
    if (testTriangle2D_) {
        testTriangle2D_->Draw(kDefaultSpriteTex);
    }
    if (testCircle2D_) {
        testCircle2D_->Draw(kDefaultSpriteTex);
    }
    if (testRing2D_) {
        testRing2D_->Draw(kDefaultSpriteTex);
    }
    if (testLine2D_) {
        testLine2D_->Draw(kDefaultSpriteTex);
    }
}
