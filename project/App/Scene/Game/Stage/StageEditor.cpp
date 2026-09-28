#include "App/Scene/Game/Stage/StageEditor.h"
#include "Engine/Debug/SceneHierarchy.h"
#include "Engine/Debug/GameViewWindow.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <direct.h> // CreateDirectoryA用

void StageEditor::Initialize(std::vector<std::unique_ptr<Enemy>>* enemies) {
    enemies_ = enemies;
    selectedEnemyIndex_ = -1;
    showStageEditor_ = true;
}

void StageEditor::Update() {
    if (!enemies_) return;

#ifdef _USEIMGUI
    if (selectedEnemyIndex_ < 0 || selectedEnemyIndex_ >= static_cast<int>(enemies_->size())) {
        selectedEnemyIndex_ = -1;
    }

    // ギズモでオブジェクトが選択された場合の双方向同期
    auto selectedObj = SceneHierarchy::GetInstance()->GetSelected();
    if (selectedObj) {
        bool found = false;
        for (int i = 0; i < static_cast<int>(enemies_->size()); ++i) {
            if ((*enemies_)[i] && (*enemies_)[i]->GetCube() == selectedObj) {
                selectedEnemyIndex_ = i;
                found = true;
                break;
            }
        }
        if (!found) {
            // 敵以外のオブジェクトが選択された場合はリストの選択を外す
            selectedEnemyIndex_ = -1;
        }
    } else {
        selectedEnemyIndex_ = -1;
    }
#endif
}

#ifdef _USEIMGUI
void StageEditor::AddEnemyFrom2D(const Vector3& pos, Enemy::EnemyType type, int count, float radius, float hangTime) {
    if (!enemies_) return;

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize();
    enemy->SetPosition(pos);
    enemy->SetSpawnPoint(true);
    enemy->SetEnemyType(type);
    enemy->SetSpawnCount(count);
    enemy->SetSpawnRadius(radius);
    enemy->SetHangTime(hangTime);
    enemy->SetSize({ radius * 2.0f, 0.1f, radius * 2.0f }); // 可視化用サイズ

    enemies_->push_back(std::move(enemy));
    selectedEnemyIndex_ = static_cast<int>(enemies_->size()) - 1;
    SceneHierarchy::GetInstance()->SetSelected((*enemies_).back()->GetCube());
}

void StageEditor::DeleteEnemy(int index) {
    if (!enemies_ || index < 0 || index >= static_cast<int>(enemies_->size())) return;

    if (SceneHierarchy::GetInstance()->GetSelected() == (*enemies_)[index]->GetCube()) {
        SceneHierarchy::GetInstance()->SetSelected(nullptr);
    }
    enemies_->erase(enemies_->begin() + index);
    if (selectedEnemyIndex_ == index) {
        selectedEnemyIndex_ = -1;
    } else if (selectedEnemyIndex_ > index) {
        selectedEnemyIndex_--;
    }
}
#endif

void StageEditor::ImGuiControl() {
#ifdef _USEIMGUI
    if (!showStageEditor_ || !enemies_) return;

    if (ImGui::Begin("敵エディタ###Enemy Editor", &showStageEditor_)) {

    // 1. モード選択
    ImGui::Text("操作モード:");
    int modeInt = static_cast<int>(editMode_);
    if (ImGui::RadioButton("通常 (None)", &modeInt, 0)) editMode_ = EditMode::None;
    ImGui::SameLine();
    if (ImGui::RadioButton("配置モード (Place)", &modeInt, 1)) editMode_ = EditMode::Placing;
    ImGui::SameLine();
    if (ImGui::RadioButton("削除モード (Delete)", &modeInt, 2)) editMode_ = EditMode::Deleting;
    ImGui::SameLine();
    if (ImGui::RadioButton("編集モード (Edit)", &modeInt, 3)) editMode_ = EditMode::Editing;

    ImGui::Separator();

    // 2. 配置設定 (Placing用)
    ImGui::Text("配置パラメータ設定:");
    const char* typeNames[] = { "通常突進 (Normal)", "固定砲台 (Stationary)", "追従群生 (Swarm)" };
    int currentTypeIdx = static_cast<int>(pendingType_);
    if (ImGui::Combo("敵タイプ", &currentTypeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
        pendingType_ = static_cast<Enemy::EnemyType>(currentTypeIdx);
    }

    ImGui::SliderInt("スポーン数", &pendingCount_, 1, 10);
    ImGui::SliderFloat("スポーン半径", &pendingRadius_, 1.0f, 20.0f);
    if (pendingType_ == Enemy::EnemyType::Swarm) {
        ImGui::SliderFloat("滞空時間 (秒)", &pendingHangTime_, 1.0f, 10.0f);
    }

    if (editMode_ == EditMode::Placing) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "状態: 2Dマップ上を左クリックして敵を配置！");
        ImGui::Text("クリック前にマウスホイールで半径を調整できます。");
    } else {
        if (ImGui::Button("配置開始 (クリックで配置)")) {
            editMode_ = EditMode::Placing;
        }
    }

    ImGui::Separator();
    ImGui::Text("配置済み敵リスト:");

    // 敵のリスト表示
    for (int i = 0; i < static_cast<int>(enemies_->size()); ++i) {
        auto& e = (*enemies_)[i];
        std::string typeStr = "通常";
        if (e->GetEnemyType() == Enemy::EnemyType::Stationary) typeStr = "固定";
        else if (e->GetEnemyType() == Enemy::EnemyType::Swarm) typeStr = "群生";

        std::string label = "ポイント " + std::to_string(i) + " [" + typeStr + " (数:" + std::to_string(e->GetSpawnCount()) + ")]";
        bool isSelected = (selectedEnemyIndex_ == i);
        if (ImGui::Selectable(label.c_str(), isSelected)) {
            selectedEnemyIndex_ = i;
            editMode_ = EditMode::Editing;
            SceneHierarchy::GetInstance()->SetSelected(e->GetCube());
        }
    }

    // 選択中の敵の編集
    if (selectedEnemyIndex_ >= 0 && selectedEnemyIndex_ < static_cast<int>(enemies_->size())) {
        ImGui::Separator();
        ImGui::Text("選択中の敵の詳細設定:");
        auto& enemy = (*enemies_)[selectedEnemyIndex_];

        Vector3 pos = enemy->GetPosition();
        if (ImGui::DragFloat3("座標", &pos.x, 0.1f)) {
            enemy->SetPosition(pos);
        }

        int typeIdx = static_cast<int>(enemy->GetEnemyType());
        if (ImGui::Combo("敵タイプ##Selected", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
            enemy->SetEnemyType(static_cast<Enemy::EnemyType>(typeIdx));
        }

        int count = enemy->GetSpawnCount();
        if (ImGui::SliderInt("スポーン数##Selected", &count, 1, 10)) {
            enemy->SetSpawnCount(count);
        }

        float radius = enemy->GetSpawnRadius();
        if (ImGui::SliderFloat("スポーン半径##Selected", &radius, 1.0f, 20.0f)) {
            enemy->SetSpawnRadius(radius);
            enemy->SetSize({ radius * 2.0f, 0.1f, radius * 2.0f });
        }

        if (enemy->GetEnemyType() == Enemy::EnemyType::Swarm) {
            float hang = enemy->GetHangTime();
            if (ImGui::SliderFloat("滞空時間##Selected", &hang, 1.0f, 10.0f)) {
                enemy->SetHangTime(hang);
            }
        }

        if (ImGui::Button("選択中の敵を削除")) {
            DeleteEnemy(selectedEnemyIndex_);
        }
    }

    // セーブ・ロード
    ImGui::Separator();
    if (ImGui::Button("ステージ保存")) {
        SaveStage(kStageFilePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("ステージ読込")) {
        LoadStage(kStageFilePath);
    }
    }
    ImGui::End();
#endif
}

void StageEditor::SaveStage(const std::string& filepath) {
    if (!enemies_) return;

    _mkdir("resources");
    _mkdir("resources/stages");

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return;

    ofs << "[\n";
    for (size_t i = 0; i < enemies_->size(); ++i) {
        auto& enemy = (*enemies_)[i];
        Vector3 pos = enemy->GetPosition();
        int typeInt = static_cast<int>(enemy->GetEnemyType());
        int count = enemy->GetSpawnCount();
        float radius = enemy->GetSpawnRadius();
        float hang = enemy->GetHangTime();

        ofs << "  {\"pos\": [" << pos.x << ", " << pos.y << ", " << pos.z << "], "
            << "\"type\": " << typeInt << ", "
            << "\"count\": " << count << ", "
            << "\"radius\": " << radius << ", "
            << "\"hang_time\": " << hang << "}";
        if (i + 1 < enemies_->size()) {
            ofs << ",";
        }
        ofs << "\n";
    }
    ofs << "]\n";
}

namespace {
    constexpr float kDefaultSpawnRadius = 5.0f;
    constexpr float kDefaultHangTime = 3.0f;
    constexpr int kDefaultSpawnCount = 1;
    constexpr float kSpawnPointHeight = 0.1f;
    constexpr float kRadiusToDiameter = 2.0f;

    const std::string kKeyPos = "\"pos\": [";
    const std::string kKeyType = "\"type\": ";
    const std::string kKeyCount = "\"count\": ";
    const std::string kKeySize = "\"size\": [";
    const std::string kKeyRadius = "\"radius\": ";
    const std::string kKeyHangTime = "\"hang_time\": ";
}

void StageEditor::LoadStage(const std::string& filepath) {
    if (!enemies_) return;

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    enemies_->clear();
    selectedEnemyIndex_ = -1;
    SceneHierarchy::GetInstance()->SetSelected(nullptr);

    std::string line;
    while (std::getline(ifs, line)) {
        size_t posIdx = line.find(kKeyPos);
        if (posIdx == std::string::npos) continue;

        size_t posEnd = line.find("]", posIdx);
        if (posEnd == std::string::npos) continue;

        size_t posStart = posIdx + kKeyPos.length();
        if (posStart >= posEnd) continue;

        std::string posStr = line.substr(posStart, posEnd - posStart);
        float px = 0.0f, py = 0.0f, pz = 0.0f;
        if (sscanf_s(posStr.c_str(), "%f, %f, %f", &px, &py, &pz) != 3) continue;

        int typeInt = 0;
        size_t typeIdx = line.find(kKeyType);
        if (typeIdx != std::string::npos) {
            size_t valStart = typeIdx + kKeyType.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%d", &typeInt);
            }
        }

        int count = kDefaultSpawnCount;
        size_t countIdx = line.find(kKeyCount);
        if (countIdx != std::string::npos) {
            size_t valStart = countIdx + kKeyCount.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%d", &count);
            }
        } else {
            // 旧形式サイズデータからのフォールバック
            size_t sizeIdx = line.find(kKeySize);
            if (sizeIdx != std::string::npos) {
                size_t valStart = sizeIdx + kKeySize.length();
                if (valStart < line.length()) {
                    float sx = 1.0f;
                    sscanf_s(line.c_str() + valStart, "%f", &sx);
                    count = static_cast<int>(sx);
                }
            }
        }

        float radius = kDefaultSpawnRadius;
        size_t radiusIdx = line.find(kKeyRadius);
        if (radiusIdx != std::string::npos) {
            size_t valStart = radiusIdx + kKeyRadius.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%f", &radius);
            }
        }

        float hang = kDefaultHangTime;
        size_t hangIdx = line.find(kKeyHangTime);
        if (hangIdx != std::string::npos) {
            size_t valStart = hangIdx + kKeyHangTime.length();
            if (valStart < line.length()) {
                sscanf_s(line.c_str() + valStart, "%f", &hang);
            }
        }

        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize();
        enemy->SetPosition({ px, py, pz });
        enemy->SetSpawnPoint(true);
        enemy->SetEnemyType(static_cast<Enemy::EnemyType>(typeInt));
        enemy->SetSpawnCount(count);
        enemy->SetSpawnRadius(radius);
        enemy->SetHangTime(hang);
        enemy->SetSize({ radius * kRadiusToDiameter, kSpawnPointHeight, radius * kRadiusToDiameter });

        enemies_->push_back(std::move(enemy));
    }
}
