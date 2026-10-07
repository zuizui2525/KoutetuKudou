#include "Engine/Debug/Command/CommandHistory.h"

CommandHistory* CommandHistory::GetInstance() {
    static CommandHistory instance;
    return &instance;
}

void CommandHistory::PushCommand(std::unique_ptr<IEditorCommand> command) {
    if (!command) return;

    // 新規操作時はRedoスタックをクリア
    redoStack_.clear();

    // コマンドを実行
    command->Execute();

    // Undoスタックへ追加
    undoStack_.push_back(std::move(command));

    // 最大履歴数を超えたら古い履歴を削除 (マジックナンバー排除: kMaxHistorySize)
    if (undoStack_.size() > kMaxHistorySize) {
        undoStack_.erase(undoStack_.begin());
    }
}

bool CommandHistory::Undo() {
    if (undoStack_.empty()) return false;

    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();

    cmd->Undo();
    redoStack_.push_back(std::move(cmd));
    return true;
}

bool CommandHistory::Redo() {
    if (redoStack_.empty()) return false;

    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();

    cmd->Redo();
    undoStack_.push_back(std::move(cmd));
    return true;
}

void CommandHistory::Clear() {
    undoStack_.clear();
    redoStack_.clear();
}

std::string CommandHistory::GetUndoName() const {
    if (undoStack_.empty()) return "";
    return undoStack_.back()->GetName();
}

std::string CommandHistory::GetRedoName() const {
    if (redoStack_.empty()) return "";
    return redoStack_.back()->GetName();
}
