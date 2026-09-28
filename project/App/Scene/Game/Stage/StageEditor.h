#pragma once
#include <vector>
#include <memory>
#include <string>
#include "App/Scene/Game/Enemy/Enemy.h"

/**
 * @brief ステージ上の敵の配置、ImGuiエディタUI、セーブ・ロードを管理するエディタクラス
 */
class StageEditor {
public:
    StageEditor() = default;
    ~StageEditor() = default;

    void Initialize(std::vector<std::unique_ptr<Enemy>>* enemies);
    void Update();
    void ImGuiControl();

    // ステージ保存・読み込み
    void SaveStage(const std::string& filepath);
    void LoadStage(const std::string& filepath);

    // ゲッター/セッター
    bool IsShowEditor() const { return showStageEditor_; }
    void SetShowEditor(bool show) { showStageEditor_ = show; }
    bool* GetShowEditorPtr() { return &showStageEditor_; }
    const std::vector<std::unique_ptr<Enemy>>* GetEnemies() const { return enemies_; }

public:
#ifdef _USEIMGUI
    enum class EditMode {
        None,       // 通常
        Placing,    // 左クリックで配置待ちモード
        Deleting,   // 左クリックで消去モード
        Editing     // 選択敵の再設定・移動モード
    };

    EditMode GetEditMode() const { return editMode_; }
    void SetEditMode(EditMode mode) { editMode_ = mode; }

    Enemy::EnemyType GetPendingType() const { return pendingType_; }
    int GetPendingCount() const { return pendingCount_; }
    float GetPendingRadius() const { return pendingRadius_; }
    void SetPendingRadius(float radius) { pendingRadius_ = radius; }
    float GetPendingHangTime() const { return pendingHangTime_; }

    int GetSelectedIndex() const { return selectedEnemyIndex_; }
    void SetSelectedIndex(int index) { selectedEnemyIndex_ = index; }

    // 2Dマップからの操作用ヘルパー
    void AddEnemyFrom2D(const Vector3& pos, Enemy::EnemyType type, int count, float radius, float hangTime);
    void DeleteEnemy(int index);
#endif

private:
    std::vector<std::unique_ptr<Enemy>>* enemies_ = nullptr; // 敵オブジェクトリストのポインタ
    int selectedEnemyIndex_ = -1;                          // 選択中の敵のインデックス
    bool showStageEditor_ = true;                          // ステージエディタウィンドウの表示フラグ

#ifdef _USEIMGUI
    EditMode editMode_ = EditMode::None;
    Enemy::EnemyType pendingType_ = Enemy::EnemyType::Normal;
    int pendingCount_ = 1;
    float pendingRadius_ = 5.0f;
    float pendingHangTime_ = 3.0f;
#endif

    static inline const std::string kStageFilePath = "resources/stages/stage1.json"; // ステージ保存先パス
    static inline const std::string kTextureKey = "white"; // 敵テクスチャ
};
