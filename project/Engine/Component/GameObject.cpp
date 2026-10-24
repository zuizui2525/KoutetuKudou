#include "Engine/Component/GameObject.h"
#include "Engine/Component/Components/MeshRendererComponent.h"
#include "Engine/Component/Components/SpriteRendererComponent.h"
#include "Engine/Component/Components/BoxColliderComponent.h"
#include "Engine/Component/Components/SphereColliderComponent.h"
#include "Engine/Component/Components/AudioSourceComponent.h"
#include "Engine/Component/Components/ParticleEffectComponent.h"
#include "Engine/Component/Components/CameraComponent.h"
#include "Engine/Component/Components/LightComponent.h"
#include "Engine/Component/Components/TextRenderer2DComponent.h"
#include "Engine/Component/Components/TextRenderer3DComponent.h"
#include "Engine/Component/Components/RotatorComponent.h"
#include "Engine/Component/Components/SinOscillatorComponent.h"
#include "Engine/Component/Components/BlinkComponent.h"
#include "Engine/Component/Components/CameraOrbitComponent.h"
#include "Engine/Component/ScriptComponent.h"
#include "Engine/Component/ScriptRegistry.h"
#include "Engine/Base/Utils/UUIDUtility.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Zuizui.h"
#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/WindowApp/WindowApp.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstring>
#ifdef _USEIMGUI
#include <imgui.h>
#endif

namespace {
    // インスペクターUI調整用定数 (マジックナンバー排除)
    constexpr float kDragSpeed = 0.05f;
    constexpr float kMinScale = 0.001f;
    constexpr float kMaxScale = 1000.0f;
    constexpr float kButtonPadding = 5.0f;

    // 2D初期配置用定数 (マジックナンバー排除)
    constexpr float kOriginDistanceThreshold = 10.0f;
    constexpr float kDefaultCenterRatio = 0.5f;

    // 3D適正座標判定用定数 (マジックナンバー排除)
    constexpr float kMax3DReasonableDistance = 50.0f; // これを超えるX/Y座標は2Dスクリーンピクセル座標の残留と判定

    // スクリプト生成用定数・変数
    constexpr const char* kScriptsDirectory = "Scripts";
    constexpr size_t kScriptBufferLength = 64;
    char sNewScriptBuffer[kScriptBufferLength] = "";
    bool sOpenNewScriptModal = false;

    // --- AntiGravity風モダンUI インスペクター定数 (マジックナンバー排除) ---
    // カード背景色: ウィンドウ背景(0.06f)よりさらに深く沈み込んだ漆黒ダーク（引き締まった外観）
    constexpr ImVec4 kModernCardBg = { 0.035f, 0.035f, 0.045f, 0.98f };
    // カード外枠ボーダー: 繊細でシャープな1px暗色ボーダー
    constexpr ImVec4 kModernCardBorder = { 0.140f, 0.150f, 0.180f, 0.85f };
    // カード角丸
    constexpr float kModernCardRounding = 3.0f;
    // カード内部パディング
    constexpr float kCardPaddingX = 4.0f; // 内部インデントを詰めてコントロール有効幅を拡大
    constexpr float kCardPaddingY = 6.0f;
    constexpr float kCardSpacingY = 8.0f;
    constexpr float kCardExpandX = 3.0f;  // カード枠を左右へダイナミックにワイド化

    // コントロール文字飛び出し防止＆引き延ばし連動用プロポーショナル幅定数 (マジックナンバー排除)
    constexpr float kComponentItemWidthRatio = 0.45f; // 利用可能幅の45%をコントロールに割り当て、55%をラベル領域に確保
    constexpr float kComponentItemMinWidth = 100.0f;  // 最小スライダー幅
    constexpr float kComponentItemMaxWidth = 200.0f;  // 最大スライダー幅（引き延ばした際にラベルスペースが広大に広がる）

    constexpr float kTransformItemWidthRatio = 0.55f; // Transform用（3連ドラッグ）
    constexpr float kTransformItemMinWidth = 120.0f;
    constexpr float kTransformItemMaxWidth = 260.0f;

    // 一体化ヘッダー（タイトルバー）背景色: 白浮き・青灰色感を完全に排除した落ち着いた超ダーク
    constexpr ImVec4 kModernHeaderBg = { 0.070f, 0.075f, 0.090f, 1.0f };
    constexpr ImVec4 kModernHeaderHovered = { 0.110f, 0.115f, 0.135f, 1.0f };
    constexpr ImVec4 kModernHeaderActive = { 0.140f, 0.145f, 0.170f, 1.0f };

    // アクセントバー定数
    constexpr float kScriptAccentBarWidth = 2.5f;
    constexpr float kDefaultAccentBarWidth = 2.0f;
    constexpr ImVec4 kScriptAccentBar = { 0.28f, 0.45f, 0.65f, 0.85f }; // スクリプト用控えめなスレートブルー
    constexpr ImVec4 kDefaultComponentAccentBar = { 0.20f, 0.22f, 0.26f, 0.80f };



    void CreateScriptTemplateFiles(const std::string& scriptName) {
        std::filesystem::create_directories(kScriptsDirectory);

        std::filesystem::path headerPath = std::filesystem::path(kScriptsDirectory) / (scriptName + ".h");
        std::filesystem::path sourcePath = std::filesystem::path(kScriptsDirectory) / (scriptName + ".cpp");

        // .h テンプレート生成
        if (!std::filesystem::exists(headerPath)) {
            std::ofstream ofs(headerPath);
            if (ofs.is_open()) {
                ofs << "#pragma once\n";
                ofs << "#include \"Engine/Component/ScriptComponent.h\"\n\n";
                ofs << "/// <summary>\n";
                ofs << "/// 【スクリプトクラス: " << scriptName << "】\n";
                ofs << "/// UnityのMonoBehaviourに相当するユーザースクリプトコンポーネントです。\n";
                ofs << "/// \n";
                ofs << "/// ■ 基底クラス (ScriptComponent) で最初から利用可能な主なメンバー変数・関数:\n";
                ofs << "///   - GetOwner()           : このスクリプトがアタッチされている GameObject へのポインタ\n";
                ofs << "///                            (例: GetOwner()->transform_.translate で自身の座標操作)\n";
                ofs << "///   - targetObject_        : インスペクターの「Target Object」スロットから登録した外部GameObjectへのポインタ\n";
                ofs << "///   - targetUUID_          : targetObject_ の識別UUID文字列 (シーン保存・復元用)\n";
                ofs << "///   - Instantiate(prefab, pos, rot) : プレハブから動的にゲームオブジェクトを生成\n";
                ofs << "///   - Destroy(gameObject)  : ゲームオブジェクトを安全に破棄\n";
                ofs << "///   - ShakeCamera(intensity, duration) : カメラシェイク(画面揺れ演出)を発生\n";
                ofs << "///   - DrawGameObjectSlot(\"名前\", uuid, objPtr) : インスペクターにドラッグ＆ドロップ可能な参照スロットを描画\n";
                ofs << "/// </summary>\n";
                ofs << "class " << scriptName << " : public ScriptComponent {\n";
                ofs << "public:\n";
                ofs << "    " << scriptName << "() : ScriptComponent(\"" << scriptName << "\") {}\n";
                ofs << "    ~" << scriptName << "() override = default;\n\n";
                ofs << "    /// <summary>\n";
                ofs << "    /// コンポーネント生成直後に1度だけ呼ばれる初期化処理\n";
                ofs << "    /// </summary>\n";
                ofs << "    void Awake() override;\n\n";
                ofs << "    /// <summary>\n";
                ofs << "    /// 最初のフレーム更新直前に1度だけ呼ばれる初期化処理\n";
                ofs << "    /// </summary>\n";
                ofs << "    void Start() override;\n\n";
                ofs << "    /// <summary>\n";
                ofs << "    /// 毎フレーム呼ばれる更新ロジック (移動・入力処理など)\n";
                ofs << "    /// </summary>\n";
                ofs << "    void OnUpdate() override;\n\n";
                ofs << "    /// <summary>\n";
                ofs << "    /// インスペクターUI描画処理 (ImGuiでパラメータを自由に公開・編集可能)\n";
                ofs << "    /// </summary>\n";
                ofs << "    void OnInspectorGUI() override;\n\n";
                ofs << "    /// <summary>\n";
                ofs << "    /// ゲーム画面上のHUD / UI描画処理 (画面への文字やゲージの描画)\n";
                ofs << "    /// </summary>\n";
                ofs << "    void OnGUI() override;\n\n";
                ofs << "    /// <summary>\n";
                ofs << "    /// コンポーネント破棄時の後始末処理\n";
                ofs << "    /// </summary>\n";
                ofs << "    void OnDestroy() override;\n\n";
                ofs << "private:\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    // メンバ変数の宣言例 (マジックナンバーを排除し、インスペクターで調整可能に)\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    float moveSpeed_ = 5.0f;           // 移動速度\n";
                ofs << "    bool isEnabledFeature_ = true;     // 機能の有効/無効フラグ\n";
                ofs << "    Vector3 initialPosition_{};        // 開始座標のキャッシュ\n";
                ofs << "};\n";
            }
        }

        // .cpp テンプレート生成
        if (!std::filesystem::exists(sourcePath)) {
            std::ofstream ofs(sourcePath);
            if (ofs.is_open()) {
                ofs << "#include \"" << scriptName << ".h\"\n";
                ofs << "#include \"Engine/Component/ScriptRegistry.h\"\n";
                ofs << "#include \"Engine/Component/GameObject.h\"\n";
                ofs << "#ifdef _USEIMGUI\n";
                ofs << "#include <imgui.h>\n";
                ofs << "#endif\n\n";
                ofs << "// =================================================================\n";
                ofs << "// スクリプト自動登録マクロ（エディタ認識用）\n";
                ofs << "// ※この記述を残しておくことで、エディタのAdd Componentやコンテンツブラウザから即座に認識されます\n";
                ofs << "// =================================================================\n";
                ofs << "REGISTER_SCRIPT(" << scriptName << ")\n\n";
                ofs << "void " << scriptName << "::Awake() {\n";
                ofs << "    // コンポーネント生成直後の初期化処理\n";
                ofs << "    // ※自身のオーナー (GetOwner()) や初期状態のセットアップを行います\n";
                ofs << "}\n\n";
                ofs << "void " << scriptName << "::Start() {\n";
                ofs << "    // 初回フレーム更新直前の初期化処理\n";
                ofs << "    // ※他オブジェクトの検索や、シーン配置完了後の初期値取得を行います\n";
                ofs << "    if (auto* owner = GetOwner()) {\n";
                ofs << "        initialPosition_ = owner->transform_.translate;\n";
                ofs << "    }\n";
                ofs << "}\n\n";
                ofs << "void " << scriptName << "::OnUpdate() {\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    // 毎フレームの更新ロジック (移動、入力処理、状態遷移など)\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    auto* owner = GetOwner();\n";
                ofs << "    if (!owner) return;\n\n";
                ofs << "    // 【移動処理の書き方例】\n";
                ofs << "    // constexpr float kDeltaTime = 1.0f / 60.0f;\n";
                ofs << "    // owner->transform_.translate.x += moveSpeed_ * kDeltaTime;\n\n";
                ofs << "    // 【プレハブ動的生成の書き方例 (弾の発射など)】\n";
                ofs << "    // if (/* 発射条件 */) {\n";
                ofs << "    //     Vector3 spawnPos = owner->transform_.translate;\n";
                ofs << "    //     GameObject* bullet = ScriptComponent::Instantiate(\"Bullet\", spawnPos, { 0.0f, 0.0f, 0.0f });\n";
                ofs << "    // }\n\n";
                ofs << "    // 【外部オブジェクト (targetObject_) を参照する書き方例】\n";
                ofs << "    // if (targetObject_) {\n";
                ofs << "    //     Vector3 targetPos = targetObject_->transform_.translate;\n";
                ofs << "    //     // targetPosに向かって処理を行う\n";
                ofs << "    // }\n";
                ofs << "}\n\n";
                ofs << "void " << scriptName << "::OnInspectorGUI() {\n";
                ofs << "#ifdef _USEIMGUI\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    // インスペクター描画処理\n";
                ofs << "    // ※ImGuiのウィジェットを使って、エディタ上でリアルタイムにパラメータを変更できます\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    constexpr float kSpeedMin = 0.0f;\n";
                ofs << "    constexpr float kSpeedMax = 100.0f;\n";
                ofs << "    constexpr float kSpeedDragStep = 0.1f;\n\n";
                ofs << "    ImGui::Text(\"--- パラメータ設定 ---\");\n";
                ofs << "    ImGui::DragFloat(\"移動速度 (Move Speed)\", &moveSpeed_, kSpeedDragStep, kSpeedMin, kSpeedMax, \"%.2f\");\n";
                ofs << "    ImGui::Checkbox(\"機能の有効化 (Enable Feature)\", &isEnabledFeature_);\n\n";
                ofs << "    // 独自のオブジェクト参照スロットを追加したい場合の書き方例:\n";
                ofs << "    // static std::string customUUID;\n";
                ofs << "    // static GameObject* customObj = nullptr;\n";
                ofs << "    // DrawGameObjectSlot(\"Custom Target\", customUUID, customObj);\n";
                ofs << "#endif\n";
                ofs << "}\n\n";
                ofs << "void " << scriptName << "::OnGUI() {\n";
                ofs << "#ifdef _USEIMGUI\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    // ゲーム画面 (GameView) 上へのHUD / UI描画\n";
                ofs << "    // ※画面の特定位置にデバッグ情報やスコアを表示したい場合に使用します\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    // ImGui::Text(\"Player Score: 100\");\n";
                ofs << "#endif\n";
                ofs << "}\n\n";
                ofs << "void " << scriptName << "::OnDestroy() {\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "    // コンポーネント破棄時の後始末処理 (動的メモリの解放や終了通知など)\n";
                ofs << "    // -------------------------------------------------------------\n";
                ofs << "}\n";
            }
        }
    }

    void SyncGPUForDeletion() {
        if (auto engine = EngineResource::GetEngine()) {
            if (auto dxCommon = engine->GetDxCommon()) {
                dxCommon->FlushGPU();
            }
        }
    }
}

GameObject::GameObject(const std::string& name) {
    name_ = name;
    uuid_ = UUIDUtility::GenerateUUID();
    Initialize();
}

GameObject::~GameObject() {
    SyncGPUForDeletion();
    components_.clear();
}

void GameObject::RemoveComponentByIndex(size_t index) {
    if (index < components_.size()) {
        SyncGPUForDeletion();
        components_.erase(components_.begin() + index);
    }
}

void GameObject::Initialize() {
    InitializeGameObject(name_);
    UpdateMatrix();
}

void GameObject::UpdateMatrix() const {
    matWorld_ = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    isMatrixDirty_ = false;
}

bool GameObject::Has2DRenderer() const {
    return HasComponent<SpriteRendererComponent>() ||
           HasComponent<TextRenderer2DComponent>();
}

bool GameObject::Has3DRenderer() const {
    return HasComponent<MeshRendererComponent>() ||
           HasComponent<TextRenderer3DComponent>();
}

bool GameObject::IsPosition2DScale() const {
    return std::abs(transform_.translate.x) > kMax3DReasonableDistance ||
           std::abs(transform_.translate.y) > kMax3DReasonableDistance;
}

void GameObject::Ensure2DPosition(bool force) {
    if (force || Math::Length(transform_.translate) < kOriginDistanceThreshold) {
        float centerX = static_cast<float>(WindowApp::kClientWidth) * kDefaultCenterRatio;
        float centerY = static_cast<float>(WindowApp::kClientHeight) * kDefaultCenterRatio;
        transform_.translate = { centerX, centerY, 0.0f };
        isMatrixDirty_ = true;
    }
}

void GameObject::Ensure3DPosition(bool force) {
    if (force) {
        transform_.translate = { 0.0f, 0.0f, 0.0f };
        isMatrixDirty_ = true;
    }
}

void GameObject::Update() {
    if (!isVisible_) return;

    // 全アクティブコンポーネントの更新（スクリプト等が位置や回転を変更する可能性がある）
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            comp->Update();
        }
    }

    // スクリプト等の更新直後にワールド行列を即座に再計算（1フレームのブレ・遅延を完全根絶）
    if (isMatrixDirty_) {
        UpdateMatrix();
    }
}

void GameObject::UpdateEditor() {
    if (!isVisible_) return;

    // エディタ編集時も位置やテクスチャの同期が必要なレンダラー系のみ更新（アニメーション系はスキップ）
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            if (auto* tr2 = dynamic_cast<TextRenderer2DComponent*>(comp.get())) {
                tr2->Update();
            } else if (auto* tr3 = dynamic_cast<TextRenderer3DComponent*>(comp.get())) {
                tr3->Update();
            } else if (auto* mr = dynamic_cast<MeshRendererComponent*>(comp.get())) {
                mr->Update();
            } else if (auto* sr = dynamic_cast<SpriteRendererComponent*>(comp.get())) {
                sr->Update();
            }
        }
    }

    if (isMatrixDirty_) {
        UpdateMatrix();
    }
}

void GameObject::Draw() {
    if (!isVisible_) return;

    // 描画直前に必ず最新の行列に更新されていることを保証
    if (isMatrixDirty_) {
        UpdateMatrix();
    }

    // 全アクティブコンポーネントの3D描画
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            comp->Draw();
        }
    }
}

void GameObject::Draw2D() {
    if (!isVisible_) return;

    if (isMatrixDirty_) {
        UpdateMatrix();
    }

    // 全アクティブコンポーネントの2D描画
    for (auto& comp : components_) {
        if (comp && comp->IsActive()) {
            comp->Draw2D();
        }
    }
}

void GameObject::DrawInspector() {
#ifdef _USEIMGUI
    std::string idPrefix = "##" + name_;

    // Transform 操作ヘッダー（AntiGravity風モダン一体化カード）
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->ChannelsSplit(2);
    drawList->ChannelsSetCurrent(1); // 前景チャンネル（ヘッダーとコンテンツを描画）

    ImVec2 transformCardStartPos = ImGui::GetCursorScreenPos();
    float availWidth = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_Header, kModernHeaderBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kModernHeaderHovered);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, kModernHeaderActive);
    bool isTransformOpen = ImGui::CollapsingHeader(("Transform" + idPrefix).c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor(3);

    if (isTransformOpen) {
        ImGui::Spacing();
        ImGui::Indent(kCardPaddingX);

        ImGui::BeginGroup();
        float tfItemWidth = std::clamp(availWidth * kTransformItemWidthRatio, kTransformItemMinWidth, kTransformItemMaxWidth);
        ImGui::PushItemWidth(tfItemWidth);

        if (ImGui::DragFloat3(("Position" + idPrefix).c_str(), &transform_.translate.x, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
            isMatrixDirty_ = true;
        }
        if (ImGui::DragFloat3(("Rotation" + idPrefix).c_str(), &transform_.rotate.x, kDragSpeed, 0.0f, 0.0f, "%.2f")) {
            isMatrixDirty_ = true;
        }
        if (ImGui::DragFloat3(("Scale" + idPrefix).c_str(), &transform_.scale.x, kDragSpeed, kMinScale, kMaxScale, "%.2f")) {
            isMatrixDirty_ = true;
        }

        ImGui::PopItemWidth();

        if (Is2D()) {
            ImGui::Spacing();
            if (ImGui::Button(("画面中央に配置 (2D)##Center_" + name_).c_str(), ImVec2(-1.0f, 0.0f))) {
                Ensure2DPosition(true);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("2D画面の中央 (X: 640, Y: 360) に配置します");
            }
        } else {
            // 3Dオブジェクト（またはレンダラー未設定時）
            if (IsPosition2DScale()) {
                ImGui::Spacing();
                static constexpr ImVec4 kWarningTextColor = { 1.0f, 0.8f, 0.2f, 1.0f }; // 黄色警告テキスト
                static constexpr ImVec4 kWarningButtonColor = { 0.8f, 0.45f, 0.1f, 1.0f }; // オレンジ強調ボタン
                ImGui::PushStyleColor(ImGuiCol_Text, kWarningTextColor);
                ImGui::TextWrapped("※ 座標が2D画面座標のままの可能性があります (X: %.1f, Y: %.1f)", transform_.translate.x, transform_.translate.y);
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Button, kWarningButtonColor);
                if (ImGui::Button(("3D原点 (0, 0, 0) にリセット##Reset3D_" + name_).c_str(), ImVec2(-1.0f, 0.0f))) {
                    Ensure3DPosition(true);
                }
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("3D空間の原点 (0, 0, 0) に座標をリセットし、カメラ視界内に戻します");
                }
            } else {
                ImGui::Spacing();
                if (ImGui::Button(("原点 (0, 0, 0) に配置##Origin3D_" + name_).c_str(), ImVec2(-1.0f, 0.0f))) {
                    Ensure3DPosition(true);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("座標を (0, 0, 0) にリセットします");
                }
            }
        }

        ImGui::Spacing();
        ImGui::EndGroup();
        ImGui::Unindent(kCardPaddingX);

        ImVec2 cardEndPos = ImGui::GetItemRectMax();
        ImVec2 rectMin = ImVec2(transformCardStartPos.x - kCardExpandX, transformCardStartPos.y);
        ImVec2 rectMax = ImVec2(transformCardStartPos.x + availWidth + kCardExpandX, cardEndPos.y + kCardPaddingY * 0.5f);

        // 背景チャンネルに切り替えて、コントロールの背面に確実に描画（濁り・白浮きの根絶）
        drawList->ChannelsSetCurrent(0);
        drawList->AddRectFilled(rectMin, rectMax, ImGui::ColorConvertFloat4ToU32(kModernCardBg), kModernCardRounding);
        drawList->AddRect(rectMin, rectMax, ImGui::ColorConvertFloat4ToU32(kModernCardBorder), kModernCardRounding);
        drawList->AddRectFilled(rectMin, ImVec2(rectMin.x + kDefaultAccentBarWidth, rectMax.y),
                                ImGui::ColorConvertFloat4ToU32(kDefaultComponentAccentBar), kModernCardRounding, ImDrawFlags_RoundCornersLeft);

        ImGui::Spacing();
    }
    drawList->ChannelsMerge();

    if (isMatrixDirty_) {
        UpdateMatrix();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // 保持している各コンポーネントの描画
    int removeIndex = -1;
    for (size_t i = 0; i < components_.size(); ++i) {
        auto& comp = components_[i];
        if (!comp) continue;

        ImGui::PushID(static_cast<int>(i));

        auto* scriptComp = dynamic_cast<ScriptComponent*>(comp.get());
        bool isScript = (scriptComp != nullptr);

        std::string headerName;
        if (isScript) {
            std::string sName = scriptComp->GetScriptName();
            if (sName.empty()) {
                sName = comp->GetComponentTypeName();
            }
            headerName = "[Script] " + sName;
        } else {
            headerName = comp->GetComponentTypeName();
        }

        // 前景チャンネル（ヘッダーと内部コンテンツ）
        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);

        ImVec2 cardStartPos = ImGui::GetCursorScreenPos();
        float compAvailWidth = ImGui::GetContentRegionAvail().x;

        // 一体化ヘッダー背景色（全コンポーネント・スクリプトで統一された落ち着いた超ダーク）
        ImGui::PushStyleColor(ImGuiCol_Header, kModernHeaderBg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kModernHeaderHovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, kModernHeaderActive);

        bool isComponentAlive = true;
        // ImGui公式のクローズボックス付きCollapsingHeaderを使用
        bool isHeaderOpen = ImGui::CollapsingHeader(headerName.c_str(), &isComponentAlive, ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::PopStyleColor(3);

        if (!isComponentAlive) {
            removeIndex = static_cast<int>(i);
        }

        if (isHeaderOpen && isComponentAlive) {
            ImGui::Spacing();
            ImGui::Indent(kCardPaddingX);

            ImGui::BeginGroup();

            // アクティブトグル
            bool active = comp->IsActive();
            if (ImGui::Checkbox("Enabled", &active)) {
                comp->SetActive(active);
            }

            // コンポーネント独自のインスペクターUI呼び出し（引き延ばしに連動してラベル領域が拡大し文字見切れを完全根絶）
            float compItemWidth = std::clamp(compAvailWidth * kComponentItemWidthRatio, kComponentItemMinWidth, kComponentItemMaxWidth);
            ImGui::PushItemWidth(compItemWidth);
            comp->DrawInspector();
            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::EndGroup();

            ImGui::Unindent(kCardPaddingX);

            // ヘッダー天面（cardStartPos.y）からコンテンツ最下部まで一体のカード枠を描画
            ImVec2 cardEndPos = ImGui::GetItemRectMax();
            ImVec2 rectMin = ImVec2(cardStartPos.x - kCardExpandX, cardStartPos.y);
            ImVec2 rectMax = ImVec2(cardStartPos.x + compAvailWidth + kCardExpandX, cardEndPos.y + kCardPaddingY * 0.5f);

            // 背景チャンネルに切り替えて、コントロールの背面に確実に描画
            drawList->ChannelsSetCurrent(0);

            float barWidth = isScript ? kScriptAccentBarWidth : kDefaultAccentBarWidth;
            ImVec4 barCol = isScript ? kScriptAccentBar : kDefaultComponentAccentBar;

            drawList->AddRectFilled(rectMin, rectMax, ImGui::ColorConvertFloat4ToU32(kModernCardBg), kModernCardRounding);
            drawList->AddRect(rectMin, rectMax, ImGui::ColorConvertFloat4ToU32(kModernCardBorder), kModernCardRounding);
            drawList->AddRectFilled(rectMin, ImVec2(rectMin.x + barWidth, rectMax.y),
                                    ImGui::ColorConvertFloat4ToU32(barCol), kModernCardRounding, ImDrawFlags_RoundCornersLeft);

            ImGui::Spacing();
        }

        drawList->ChannelsMerge();
        ImGui::PopID();
    }

    // 削除リクエストがあったコンポーネントを安全に削除
    if (removeIndex >= 0) {
        RemoveComponentByIndex(static_cast<size_t>(removeIndex));
    }

    ImGui::Separator();
    ImGui::Spacing();

    // [+ Add Component] ボタン (UnityライクなUI)
    constexpr float kAddBtnHeight = 26.0f;
    if (ImGui::Button("+ Add Component", ImVec2(-1.0f, kAddBtnHeight))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    // スクリプトのドラッグ＆ドロップ受け取り (ContentBrowserからのスクリプトアタッチ)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_SCRIPT")) {
            const char* scriptFilePath = static_cast<const char*>(payload->Data);
            std::filesystem::path p(scriptFilePath);
            std::string stemName = p.stem().string(); // "PlayerController.cpp" -> "PlayerController"
            auto scriptInst = ScriptRegistry::GetInstance().CreateScript(stemName);
            AddComponentInstance(std::move(scriptInst));
        }
        ImGui::EndDragDropTarget();
    }

    // サブメニューが右側（矢印の方向）に展開できるよう、右側余白を確保した座標にポップアップを配置
    constexpr float kPopupWidthEstimate = 200.0f;
    constexpr float kSubmenuWidthEstimate = 240.0f;
    constexpr float kMargin = 10.0f;
    const float totalMenuWidth = kPopupWidthEstimate + kSubmenuWidthEstimate;

    ImVec2 buttonMin = ImGui::GetItemRectMin();
    ImVec2 buttonMax = ImGui::GetItemRectMax();
    float displayRight = ImGui::GetIO().DisplaySize.x;
    float popupX = buttonMin.x;

    if (popupX + totalMenuWidth > displayRight) {
        popupX = (std::max)(kMargin, displayRight - totalMenuWidth - kMargin);
    }

    ImGui::SetNextWindowPos(ImVec2(popupX, buttonMax.y));
    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("-- Select Component --");
        ImGui::Separator();

        // コンポーネント選択肢
        if (ImGui::BeginMenu("Audio")) {
            if (ImGui::MenuItem("AudioSource (サウンド再生)")) {
                AddComponent<AudioSourceComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Physics / Collision")) {
            if (ImGui::MenuItem("BoxCollider (箱型当たり判定)")) {
                AddComponent<BoxColliderComponent>();
            }
            if (ImGui::MenuItem("SphereCollider (球型当たり判定)")) {
                AddComponent<SphereColliderComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Effects")) {
            if (ImGui::MenuItem("ParticleEffect (エフェクト発生)")) {
                AddComponent<ParticleEffectComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Rendering")) {
            const bool has2D = Has2DRenderer();
            const bool has3D = Has3DRenderer();

            // 3Dレンダラー（2Dレンダラー保持時は排他制御で無効化）
            if (ImGui::MenuItem("MeshRenderer (3D形状描画)", nullptr, false, !has2D)) {
                AddComponent<MeshRendererComponent>();
            }
            if (has2D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("2Dレンダラーを保持しているため、3Dレンダラーは追加できません");
            }

            if (ImGui::MenuItem("TextRenderer3D (3Dテキスト描画)", nullptr, false, !has2D)) {
                AddComponent<TextRenderer3DComponent>();
            }
            if (has2D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("2Dレンダラーを保持しているため、3Dレンダラーは追加できません");
            }

            // 2Dレンダラー（3Dレンダラー保持時は排他制御で無効化）
            if (ImGui::MenuItem("SpriteRenderer (2Dスプライト描画)", nullptr, false, !has3D)) {
                AddComponent<SpriteRendererComponent>();
                Ensure2DPosition();
            }
            if (has3D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("3Dレンダラーを保持しているため、2Dレンダラーは追加できません");
            }

            if (ImGui::MenuItem("TextRenderer2D (2Dテキスト描画)", nullptr, false, !has3D)) {
                AddComponent<TextRenderer2DComponent>();
                Ensure2DPosition();
            }
            if (has3D && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("3Dレンダラーを保持しているため、2Dレンダラーは追加できません");
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Camera (カメラ)")) {
                AddComponent<CameraComponent>();
            }
            if (ImGui::MenuItem("Light (光源)")) {
                AddComponent<LightComponent>();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Animation / Motion")) {
            if (ImGui::MenuItem("Rotator (連続回転)")) {
                AddComponent<RotatorComponent>();
            }
            if (ImGui::MenuItem("SinOscillator (浮遊・往復運動)")) {
                AddComponent<SinOscillatorComponent>();
            }
            if (ImGui::MenuItem("Blink (点滅・明滅)")) {
                AddComponent<BlinkComponent>();
            }
            if (ImGui::MenuItem("CameraOrbit (カメラ自動旋回)")) {
                AddComponent<CameraOrbitComponent>();
            }
            ImGui::EndMenu();
        }

        // スクリプト追加メニュー (最下部に配置)
        ImGui::Separator();

        auto registeredScripts = ScriptRegistry::GetInstance().GetRegisteredScriptNames();
        if (!registeredScripts.empty()) {
            if (ImGui::BeginMenu("Scripts (登録済みスクリプト)")) {
                for (const auto& sName : registeredScripts) {
                    if (ImGui::MenuItem(sName.c_str())) {
                        auto scriptInst = ScriptRegistry::GetInstance().CreateScript(sName);
                        AddComponentInstance(std::move(scriptInst));
                    }
                }
                ImGui::EndMenu();
            }
        }

        // Add Component の一番下の [+ New Script...] 項目
        if (ImGui::MenuItem("+ New Script... (新規スクリプト作成)")) {
            sOpenNewScriptModal = true;
            std::memset(sNewScriptBuffer, 0, sizeof(sNewScriptBuffer));
        }

        ImGui::EndPopup();
    }

    // スクリプト名入力モーダルダイアログ
    if (sOpenNewScriptModal) {
        ImGui::OpenPopup("NewScriptModal");
        sOpenNewScriptModal = false;
    }

    if (ImGui::BeginPopupModal("NewScriptModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("新しいスクリプト名を入力してください:");
        ImGui::Spacing();

        constexpr float kInputWidth = 260.0f;
        constexpr float kModalBtnWidth = 130.0f;
        constexpr float kModalCancelBtnWidth = 90.0f;

        ImGui::SetNextItemWidth(kInputWidth);
        ImGui::InputText("##ScriptNameInput", sNewScriptBuffer, sizeof(sNewScriptBuffer));

        bool hasValidName = (std::strlen(sNewScriptBuffer) > 0);
        ImGui::Spacing();

        if (!hasValidName) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("作成してアタッチ", ImVec2(kModalBtnWidth, 0.0f))) {
            std::string scriptName = sNewScriptBuffer;
            // テンプレートファイルを自動生成
            CreateScriptTemplateFiles(scriptName);

            // スクリプトコンポーネントを即時生成してアタッチ
            auto scriptInst = ScriptRegistry::GetInstance().CreateScript(scriptName);
            AddComponentInstance(std::move(scriptInst));

            ImGui::CloseCurrentPopup();
        }

        if (!hasValidName) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(kModalCancelBtnWidth, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
#endif
}
