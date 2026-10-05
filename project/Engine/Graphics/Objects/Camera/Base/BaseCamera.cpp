#include "Engine/Graphics/Objects/Camera/Base/BaseCamera.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include "ImGuiManager.h"

void BaseCamera::Initialize() {
    transform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,-20.0f} };

    // 射影行列の初期化
    projectionMatrix_ = Math::MakePerspectiveFovMatrix(
        0.45f,
        static_cast<float>(WindowApp::kClientWidth) / static_cast<float>(WindowApp::kClientHeight),
        0.1f, 1000.0f
    );

    // 内部カメラはSceneHierarchyに登録しない（GameObject側で管理）
}

BaseCamera::~BaseCamera() = default;

void BaseCamera::Update() {
    if (useTarget_) {
        // 注視点モード
        viewMatrix_ = Math::MakeLookAtMatrix(transform_.translate, target_, { 0.0f, 1.0f, 0.0f });
    } else {
        // 回転モード
        Matrix4x4 cameraMatrix = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
        viewMatrix_ = Math::Inverse(cameraMatrix);
    }
}
void BaseCamera::UpdateProjection(float aspect) {
    aspectRatio_ = aspect;
    projectionMatrix_ = Math::MakePerspectiveFovMatrix(fov_, aspectRatio_, nearZ_, farZ_);
}

void BaseCamera::DrawInspector() {
#ifdef _USEIMGUI
    std::string tag = "##" + name_;
    if (ImGui::CollapsingHeader(("Transform" + tag).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3(("Pos" + tag).c_str(), &transform_.translate.x, 0.1f, 0, 0, "%.1f");
        ImGui::DragFloat3(("Rot" + tag).c_str(), &transform_.rotate.x, 0.1f, 0, 0, "%.1f");
    }
#endif
}

void BaseCamera::CreateRay(const Vector2& screenPos, float windowWidth, float windowHeight, Vector3& rayStart, Vector3& rayDir) const {
    // マジックナンバーを排除したローカル定数
    const float kNDCMax = 1.0f;
    const float kTwo = 2.0f;
    const float kZero = 0.0f;

    // 1. スクリーン座標をNDC（正規化デバイス座標）に変換
    float ndcX = (kTwo * screenPos.x) / windowWidth - kNDCMax;
    float ndcY = kNDCMax - (kTwo * screenPos.y) / windowHeight;

    // 2. ビュープロジェクション行列の逆行列を作成
    Matrix4x4 vp = Math::Multiply(viewMatrix_, projectionMatrix_);
    Matrix4x4 invVP = Math::Inverse(vp);

    // 3. Near平面とFar平面上の点をワールド空間に逆投影
    Vector4 nearNDC = { ndcX, ndcY, kZero, kNDCMax };
    Vector4 farNDC = { ndcX, ndcY, kNDCMax, kNDCMax };

    // 行列乗算によりワールド座標を計算
    auto TransformVec4 = [](const Matrix4x4& m, const Vector4& v) -> Vector4 {
        Vector4 result;
        result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
        result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
        result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
        result.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
        return result;
    };

    Vector4 nearWorld = TransformVec4(invVP, nearNDC);
    Vector4 farWorld = TransformVec4(invVP, farNDC);

    // w成分で割って同次座標から3D座標へ変換
    if (nearWorld.w != kZero) {
        nearWorld.x /= nearWorld.w;
        nearWorld.y /= nearWorld.w;
        nearWorld.z /= nearWorld.w;
    }
    if (farWorld.w != kZero) {
        farWorld.x /= farWorld.w;
        farWorld.y /= farWorld.w;
        farWorld.z /= farWorld.w;
    }

    rayStart = { nearWorld.x, nearWorld.y, nearWorld.z };
    Vector3 farPos = { farWorld.x, farWorld.y, farWorld.z };
    rayDir = Math::Normalize(Math::Subtract(farPos, rayStart));
}

