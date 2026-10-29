#include "Scripts/Stage/SkydomeScript.h"
#include "Engine/Graphics/Objects/3d/Skydome/Skydome.h"
#include "Engine/Component/ScriptRegistry.h"
#ifdef _USEIMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
    constexpr size_t kKeyBufferSize = 128;
    const std::string kDefaultModelKey = "skydome";
    const std::string kDefaultTextureKey = "skydome";
}

REGISTER_SCRIPT(SkydomeScript);

SkydomeScript::SkydomeScript()
    : ScriptComponent("SkydomeScript")
    , modelKey_(kDefaultModelKey)
    , textureKey_(kDefaultTextureKey) {
}

SkydomeScript::~SkydomeScript() = default;

void SkydomeScript::SetRotationSpeed(float speed) {
    rotationSpeed_ = speed;
    if (skydome_) {
        skydome_->SetRotationSpeed(speed);
    }
}

void SkydomeScript::SetColor(const Vector4& color) {
    color_ = color;
    if (skydome_) {
        skydome_->SetColor(color);
    }
}

void SkydomeScript::SetModelKey(const std::string& key) {
    modelKey_ = key;
    if (skydome_) {
        skydome_->Initialize(modelKey_);
    }
}

void SkydomeScript::SetTextureKey(const std::string& key) {
    textureKey_ = key;
}

void SkydomeScript::EnsureSkydome() {
    if (!skydome_) {
        skydome_ = std::make_unique<Skydome>();
        skydome_->Initialize(modelKey_);
        skydome_->SetRotationSpeed(rotationSpeed_);
        skydome_->SetColor(color_);
    }
}

void SkydomeScript::Awake() {
    EnsureSkydome();
}

void SkydomeScript::Start() {
    EnsureSkydome();
}

void SkydomeScript::OnUpdate() {
    EnsureSkydome();
    if (skydome_) {
        skydome_->Update();
    }
}

void SkydomeScript::Draw() {
    EnsureSkydome();
    if (skydome_) {
        // エディタ停止中（ポーズ中）でもカメラ移動に正確に追従させるため、描画直前にも位置行列を同期
        skydome_->Update();
        skydome_->Draw(textureKey_);
    }
}

void SkydomeScript::OnInspectorGUI() {
#ifdef _USEIMGUI
    char modelBuf[kKeyBufferSize];
    strncpy_s(modelBuf, modelKey_.c_str(), sizeof(modelBuf) - 1);
    if (ImGui::InputText("モデルキー###ModelKey", modelBuf, sizeof(modelBuf))) {
        SetModelKey(modelBuf);
    }

    char texBuf[kKeyBufferSize];
    strncpy_s(texBuf, textureKey_.c_str(), sizeof(texBuf) - 1);
    if (ImGui::InputText("テクスチャキー###TextureKey", texBuf, sizeof(texBuf))) {
        SetTextureKey(texBuf);
    }

    if (ImGui::DragFloat("自転速度###RotSpeed", &rotationSpeed_, 0.0001f, -0.1f, 0.1f, "%.5f")) {
        SetRotationSpeed(rotationSpeed_);
    }

    if (ImGui::ColorEdit4("天球カラー###Color", &color_.x)) {
        SetColor(color_);
    }

    if (skydome_) {
        Vector3 scale = skydome_->GetScale();
        if (ImGui::DragFloat3("スケール###Scale", &scale.x, 10.0f, 10.0f, 10000.0f)) {
            skydome_->SetScale(scale);
        }
    }
#endif
}
