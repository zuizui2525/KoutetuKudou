#pragma once
#include <vector>
#include <memory>
#include <string>
#include "App/Scene/Game/Enemy/Enemy.h"

class BaseCamera;
class DrawRouteCamera;

/**
 * @brief ステージ上の敵の配置、ImGuiエディタUI、セーブ・ロードを管理するエディタクラス
 */
class StageEditor {
public:
    StageEditor() = default;
    ~StageEditor() = default;

    void Initialize();
    void Update();
    void ImGuiControl();
    void Draw3D(); // 湧き範囲サークルの3D描画

    // ステージ保存・読み込み
    void SaveStage(const std::string& filepath);
    void LoadStage(const std::string& filepath);

    // ゲッター/セッター
    bool IsShowEditor() const { return showStageEditor_; }
    void SetShowEditor(bool show) { showStageEditor_ = show; }
    bool* GetShowEditorPtr() { return &showStageEditor_; }
    const std::vector<std::unique_ptr<Enemy>>& GetEnemies() const { return editorEnemies_; }
    std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return editorEnemies_; }

    // ズームカメラ連携（敵選択時の自動フォーカス用）
    void SetDrawRouteCamera(DrawRouteCamera* cam) { drawRouteCamera_ = cam; }

    // ガイド表示設定
    bool IsShowSpawnCircles() const { return showSpawnCircles_; }
    bool IsShow3DLabels() const { return show3DLabels_; }
    bool IsShowDuringPlay() const { return showDuringPlay_; }

    // 画面分割時の3Dビューポート比率設定 (オフセット比率, 幅比率)
    void SetViewportRatio(float offsetXRatio, float widthRatio) {
        vpOffsetXRatio_ = offsetXRatio;
        vpWidthRatio_ = widthRatio;
    }

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
    void SetSelectedIndex(int index);

    // 2Dマップおよび3Dマップからの配置用ヘルパー
    void AddEnemyFrom2D(const Vector3& pos, Enemy::EnemyType type, int count, float radius, float hangTime);
    void DeleteEnemy(int index);

    // 3Dテキスト（ビルボードバッジ）描画
    void Draw3DLabels();

    // 3D地面レイキャストとクリック配置処理
    void Update3DPlacement(BaseCamera* camera);

    // 配置中かどうかの静的判定（GameViewWindow等の入力排他制御用）
    static bool IsPlacingNow() { return sIsPlacingNow_; }
#endif

private:
    std::vector<std::unique_ptr<Enemy>> editorEnemies_;    // エディタ配置敵オブジェクトリスト
    int selectedEnemyIndex_ = -1;                          // 選択中の敵のインデックス
    bool showStageEditor_ = true;                          // ステージエディタウィンドウの表示フラグ
    bool showSpawnCircles_ = true;                         // 湧き範囲サークル表示フラグ
    bool show3DLabels_ = true;                             // 3Dテキスト表示フラグ
    bool showDuringPlay_ = true;                           // プレイ中もガイドを表示するフラグ
    float vpOffsetXRatio_ = 0.0f;                          // 3Dビューポートの左オフセット比率 (通常0.0f, 分割時0.3f)
    float vpWidthRatio_ = 1.0f;                            // 3Dビューポートの幅比率 (通常1.0f, 分割時0.7f)
    DrawRouteCamera* drawRouteCamera_ = nullptr;           // ズームカメラへの参照ポインタ

#ifdef _USEIMGUI
    static inline bool sIsPlacingNow_ = false;             // 現在配置モード中かどうかの静的フラグ
    EditMode editMode_ = EditMode::None;
    Enemy::EnemyType pendingType_ = Enemy::EnemyType::Normal;
    int pendingCount_ = 1;
    float pendingRadius_ = 5.0f;
    float pendingHangTime_ = 3.0f;

    // 3D地面カーソル交差情報
    bool isCursorOnGround_ = false;
    Vector3 cursorGroundPos_ = { 0.0f, 0.0f, 0.0f };

    // マジックナンバー排除のための3Dラベル定数
    static inline const float kLabelHeightOffset = 1.8f;
    static inline const float kBadgePaddingX = 8.0f;
    static inline const float kBadgePaddingY = 4.0f;
    static inline const float kBadgeRounding = 4.0f;
    static inline const float kMaxLabelVisibleDist = 120.0f;
    static inline const float kGroundMarkerRadius = 3.0f;
    static inline const float kLeadLineWidth = 1.5f;
    static inline const float kSelectedBorderWidth = 3.0f;
    static inline const float kNormalBorderWidth = 1.0f;

    // マジックナンバー排除のための3Dサークル定数
    static inline const float kCircleElevation = 0.05f;          // 地面との干渉防止微小オフセット
    static inline const float kCircleLineWidth = 2.0f;           // サークル枠線の太さ
    static inline const float kSelectedCircleLineWidth = 4.5f;    // 選択中サークル枠線の太さ（強調表示）
    static inline const int kCircleSegments = 36;                // サークル分割数

    // マジックナンバー排除のための3D配置定数
    static inline const float kGroundLevelY = 0.0f;               // 地面高さ
    static inline const float kRayDirThreshold = 1e-4f;          // レイ傾斜判定しきい値
    static inline const float kMinSpawnRadius = 1.0f;            // 最小湧き半径
    static inline const float kMaxSpawnRadius = 20.0f;           // 最大湧き半径
    static inline const float kRadiusWheelStep = 0.5f;           // ホイール1目盛りの増減量
#endif

    static inline const std::string kStageFilePath = "resources/stages/stage1.json"; // ステージ保存先パス
    static inline const std::string kTextureKey = "white"; // 敵テクスチャ
};
