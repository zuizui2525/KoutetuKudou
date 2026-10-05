#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/GameObject.h"
#include "Engine/Graphics/Objects/3d/Cube/CubeObject.h"
#include "Engine/Graphics/Objects/3d/Sphere/SphereObject.h"
#include "Engine/Graphics/Objects/3d/Pyramid/PyramidObject.h"
#include "Engine/Graphics/Objects/3d/TriangularPyramid/TriangularPyramidObject.h"
#include "Engine/Graphics/Objects/3d/Triangle/TriangleObject.h"
#include "Engine/Graphics/Objects/3d/Square/SquareObject.h"
#include "Engine/Graphics/Objects/3d/Cylinder/CylinderObject.h"
#include "Engine/Graphics/Objects/3d/Cone/ConeObject.h"
#include "Engine/Graphics/Objects/3d/Ring/RingObject.h"
#include "Engine/Graphics/Objects/3d/Hemisphere/HemisphereObject.h"
#include "Engine/Graphics/Objects/3d/Model/ModelObject.h"
#include "Engine/Graphics/Objects/3d/Line/LineObject.h"
#include "Engine/Graphics/Objects/3d/FlatRing/FlatRingObject.h"
#include "Engine/Graphics/Objects/3d/CylinderEffect/CylinderEffectObject.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Zuizui.h"

#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクター編集用バッファサイズおよび調整刻み値 (マジックナンバー排除)
    constexpr size_t kTextBufferSize = 128;
    constexpr float kShininessSpeed = 1.0f;
    constexpr float kMinShininess = 1.0f;
    constexpr float kMaxShininess = 200.0f;
    constexpr const char* kMeshTypeNames[] = {
        "Cube",
        "Sphere",
        "Pyramid",
        "TriangularPyramid",
        "Triangle",
        "Square",
        "Cylinder",
        "Cone",
        "Ring",
        "Hemisphere",
        "Model",
        "Line",
        "FlatRing",
        "CylinderEffect"
    };
    constexpr int kMeshTypeCount = sizeof(kMeshTypeNames) / sizeof(kMeshTypeNames[0]);
    constexpr const char* kLightingModeNames[] = { "None", "Lambert", "HalfLambert" };
    constexpr int kLightingModeCount = sizeof(kLightingModeNames) / sizeof(kLightingModeNames[0]);
}

MeshRendererComponent::MeshRendererComponent() {
    RecreateMeshObject();
}

MeshRendererComponent::~MeshRendererComponent() {
    // コンポーネント破棄時にGPU処理完了を待機（リソース破棄クラッシュを完全防止）
    if (auto engine = EngineResource::GetEngine()) {
        if (auto dxCommon = engine->GetDxCommon()) {
            dxCommon->FlushGPU();
        }
    }
}

void MeshRendererComponent::Initialize() {
    if (meshObject_) {
        meshObject_->SetColor(color_);
        meshObject_->SetShininess(shininess_);
        meshObject_->SetLightingMode(lightingMode_);
    }
}

void MeshRendererComponent::RecreateMeshObject() {
    switch (meshType_) {
    case MeshType::Cube:
        meshObject_ = std::make_unique<CubeObject>();
        break;
    case MeshType::Sphere:
        meshObject_ = std::make_unique<SphereObject>();
        break;
    case MeshType::Pyramid:
        meshObject_ = std::make_unique<PyramidObject>();
        break;
    case MeshType::TriangularPyramid:
        meshObject_ = std::make_unique<TriangularPyramidObject>();
        break;
    case MeshType::Triangle:
        meshObject_ = std::make_unique<TriangleObject>();
        break;
    case MeshType::Square:
        meshObject_ = std::make_unique<SquareObject>();
        break;
    case MeshType::Cylinder:
        meshObject_ = std::make_unique<CylinderObject>();
        break;
    case MeshType::Cone:
        meshObject_ = std::make_unique<ConeObject>();
        break;
    case MeshType::Ring:
        meshObject_ = std::make_unique<RingObject>();
        break;
    case MeshType::Hemisphere:
        meshObject_ = std::make_unique<HemisphereObject>();
        break;
    case MeshType::Model:
        meshObject_ = std::make_unique<ModelObject>();
        break;
    case MeshType::Line:
        meshObject_ = std::make_unique<LineObject>();
        break;
    case MeshType::FlatRing:
        meshObject_ = std::make_unique<FlatRingObject>();
        break;
    case MeshType::CylinderEffect:
        meshObject_ = std::make_unique<CylinderEffectObject>();
        break;
    default:
        meshObject_ = std::make_unique<CubeObject>();
        break;
    }

    if (meshObject_) {
        meshObject_->SetAutoRegisterHierarchy(false);
        meshObject_->Initialize(lightingMode_);
        meshObject_->SetColor(color_);
        meshObject_->SetShininess(shininess_);

        // コンポーネント内部の描画メッシュなのでヒエラルキーからは即座に登録解除（余計なCubeが階層に出るのを防止）
        SceneHierarchy::GetInstance()->Unregister(meshObject_.get());
    }
}

void MeshRendererComponent::SetMeshType(MeshType type) {
    if (meshType_ != type || !meshObject_) {
        // メッシュ切り替え前にGPU処理の完了を待機（リソース破棄クラッシュ OBJECT_DELETED を完全防止）
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
        meshType_ = type;
        RecreateMeshObject();
    }
}

void MeshRendererComponent::SetColor(const Vector4& color) {
    color_ = color;
    if (meshObject_) {
        meshObject_->SetColor(color_);
    }
}

void MeshRendererComponent::SetLightingMode(int mode) {
    lightingMode_ = mode;
    if (meshObject_) {
        meshObject_->SetLightingMode(lightingMode_);
    }
}

void MeshRendererComponent::SetShininess(float shininess) {
    shininess_ = shininess;
    if (meshObject_) {
        meshObject_->SetShininess(shininess_);
    }
}

void MeshRendererComponent::Update() {
    if (!meshObject_ || !owner_) return;

    // オーナーのTransformと表示状態をメッシュオブジェクトに同期
    meshObject_->SetTransform(owner_->GetTransform());
    meshObject_->SetVisible(owner_->IsVisible() && isActive_);

    // メッシュ側の行列（WVP）を確実に計算・更新
    meshObject_->Update();
}

void MeshRendererComponent::Draw() {
    if (!meshObject_ || !isActive_ || !owner_ || !owner_->IsVisible()) return;

    // 描画直前にTransformと行列を最新化して確実に描画
    meshObject_->SetTransform(owner_->GetTransform());
    meshObject_->SetVisible(true);
    meshObject_->Update();

    std::string drawTexKey = textureKey_.empty() ? "white" : textureKey_;

    if (meshType_ == MeshType::Model) {
        if (auto* model = dynamic_cast<ModelObject*>(meshObject_.get())) {
            model->Draw(modelKey_, drawTexKey, envMapKey_);
        }
    } else {
        if (auto* cube = dynamic_cast<CubeObject*>(meshObject_.get())) {
            cube->Draw(drawTexKey, envMapKey_);
        } else if (auto* sphere = dynamic_cast<SphereObject*>(meshObject_.get())) {
            sphere->Draw(drawTexKey, envMapKey_);
        } else if (auto* pyramid = dynamic_cast<PyramidObject*>(meshObject_.get())) {
            pyramid->Draw(drawTexKey, envMapKey_);
        } else if (auto* tp = dynamic_cast<TriangularPyramidObject*>(meshObject_.get())) {
            tp->Draw(drawTexKey, envMapKey_);
        } else if (auto* tri = dynamic_cast<TriangleObject*>(meshObject_.get())) {
            tri->Draw(drawTexKey, envMapKey_);
        } else if (auto* square = dynamic_cast<SquareObject*>(meshObject_.get())) {
            square->Draw(drawTexKey, envMapKey_);
        } else if (auto* cyl = dynamic_cast<CylinderObject*>(meshObject_.get())) {
            cyl->Draw(drawTexKey, envMapKey_);
        } else if (auto* cone = dynamic_cast<ConeObject*>(meshObject_.get())) {
            cone->Draw(drawTexKey, envMapKey_);
        } else if (auto* ring = dynamic_cast<RingObject*>(meshObject_.get())) {
            ring->Draw(drawTexKey, envMapKey_);
        } else if (auto* hemi = dynamic_cast<HemisphereObject*>(meshObject_.get())) {
            hemi->Draw(drawTexKey, envMapKey_);
        } else if (auto* line = dynamic_cast<LineObject*>(meshObject_.get())) {
            line->Draw(drawTexKey);
        } else if (auto* fr = dynamic_cast<FlatRingObject*>(meshObject_.get())) {
            fr->Draw(drawTexKey, envMapKey_);
        } else if (auto* ce = dynamic_cast<CylinderEffectObject*>(meshObject_.get())) {
            ce->Draw(drawTexKey, envMapKey_);
        }
    }
}

void MeshRendererComponent::DrawInspector() {
#ifdef _USEIMGUI
    // 1. メッシュタイプ選択コンボ
    int currentType = static_cast<int>(meshType_);
    if (ImGui::Combo("Mesh Type##MeshRenderer", &currentType, kMeshTypeNames, kMeshTypeCount)) {
        SetMeshType(static_cast<MeshType>(currentType));
    }

    // 2. モデルキー (MeshTypeがModelの場合のみ)
    if (meshType_ == MeshType::Model) {
        char modelBuf[kTextBufferSize]{};
        strncpy_s(modelBuf, modelKey_.c_str(), sizeof(modelBuf) - 1);
        if (ImGui::InputText("Model Key##MeshRenderer", modelBuf, sizeof(modelBuf))) {
            modelKey_ = modelBuf;
        }
    }

    // 3. テクスチャキー
    char texBuf[kTextBufferSize]{};
    strncpy_s(texBuf, textureKey_.c_str(), sizeof(texBuf) - 1);
    if (ImGui::InputText("Texture Key##MeshRenderer", texBuf, sizeof(texBuf))) {
        textureKey_ = texBuf;
    }

    // 4. 環境マップキー
    char envBuf[kTextBufferSize]{};
    strncpy_s(envBuf, envMapKey_.c_str(), sizeof(envBuf) - 1);
    if (ImGui::InputText("EnvMap Key##MeshRenderer", envBuf, sizeof(envBuf))) {
        envMapKey_ = envBuf;
    }

    // 5. カラーピッカー
    float col[4] = { color_.x, color_.y, color_.z, color_.w };
    if (ImGui::ColorEdit4("Color##MeshRenderer", col)) {
        SetColor({ col[0], col[1], col[2], col[3] });
    }

    // 6. ライティングモード
    int currentLighting = lightingMode_;
    if (ImGui::Combo("Lighting Mode##MeshRenderer", &currentLighting, kLightingModeNames, kLightingModeCount)) {
        SetLightingMode(currentLighting);
    }

    // 7. スペキュラ鋭さ (Shininess)
    float shininess = shininess_;
    if (ImGui::DragFloat("Shininess##MeshRenderer", &shininess, kShininessSpeed, kMinShininess, kMaxShininess, "%.1f")) {
        SetShininess(shininess);
    }
#endif
}
