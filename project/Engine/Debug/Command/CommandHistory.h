#pragma once
#include "Engine/Debug/Command/IEditorCommand.h"
#include <vector>
#include <memory>
#include <string>

/// <summary>
/// エディタ操作履歴を管理し Undo / Redo を提供するマネージャークラス
/// </summary>
class CommandHistory {
public:
    static CommandHistory* GetInstance();

    /// <summary>
    /// 新しいコマンドを実行して履歴スタックに登録
    /// </summary>
    void PushCommand(std::unique_ptr<IEditorCommand> command);

    /// <summary>
    /// 直前の操作を取り消す (Ctrl + Z)
    /// </summary>
    bool Undo();

    /// <summary>
    /// 取り消した操作をやり直す (Ctrl + Y)
    /// </summary>
    bool Redo();

    /// <summary>
    /// Undo可能かどうか
    /// </summary>
    bool CanUndo() const { return !undoStack_.empty(); }

    /// <summary>
    /// Redo可能かどうか
    /// </summary>
    bool CanRedo() const { return !redoStack_.empty(); }

    /// <summary>
    /// 全履歴をクリア（シーン切り替え時等）
    /// </summary>
    void Clear();

    /// <summary>
    /// 直近のUndo/Redoコマンド名を取得（UI表示用）
    /// </summary>
    std::string GetUndoName() const;
    std::string GetRedoName() const;

private:
    CommandHistory() = default;
    ~CommandHistory() = default;
    CommandHistory(const CommandHistory&) = delete;
    CommandHistory& operator=(const CommandHistory&) = delete;

private:
    static constexpr size_t kMaxHistorySize = 50;

    std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
    std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
};
