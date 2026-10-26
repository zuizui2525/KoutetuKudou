#pragma once
#include <string>

#ifdef _USEIMGUI

enum class NotificationType {
    Success,
    Info,
    Warning,
};

/**
 * @brief エディタ用トースト通知（通知バナー）管理クラス
 * 画面中央上部に操作完了（セーブ、シーン切替、画面サイズ変更等）の通知をポップアップ表示します。
 */
class EditorNotification {
public:
    /// @brief 通知メッセージを表示（デフォルト表示時間 2.5 秒）
    static void Show(const std::string& message, NotificationType type = NotificationType::Success, float duration = 2.5f);

    /// @brief 成功通知を即座に表示
    static void ShowSuccess(const std::string& message, float duration = 2.5f) {
        Show(message, NotificationType::Success, duration);
    }

    /// @brief 情報通知を即座に表示
    static void ShowInfo(const std::string& message, float duration = 2.5f) {
        Show(message, NotificationType::Info, duration);
    }

    /// @brief 警告通知を即座に表示
    static void ShowWarning(const std::string& message, float duration = 3.0f) {
        Show(message, NotificationType::Warning, duration);
    }

    /// @brief 毎フレームの更新・描画（DebugEditor::Draw 内から呼び出す）
    static void Draw();

private:
    struct ToastItem {
        std::string message;
        NotificationType type = NotificationType::Success;
        float remainingTime = 0.0f;
        float totalDuration = 2.5f;
    };

    static ToastItem sCurrentToast;
};

#else

// Releaseビルド等（_USEIMGUI 未定義時）の空スタブ定義
enum class NotificationType { Success, Info, Warning };
class EditorNotification {
public:
    static void Show(const std::string&, NotificationType = NotificationType::Success, float = 0.0f) {}
    static void ShowSuccess(const std::string&, float = 0.0f) {}
    static void ShowInfo(const std::string&, float = 0.0f) {}
    static void ShowWarning(const std::string&, float = 0.0f) {}
    static void Draw() {}
};

#endif