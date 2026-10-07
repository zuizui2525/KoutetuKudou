#pragma once
#include <string>

/// <summary>
/// エディタ操作の Undo / Redo を行うコマンドの基底インターフェース (Unity / Blender 準拠)
/// </summary>
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;

    /// <summary>
    /// コマンドの初回実行
    /// </summary>
    virtual void Execute() = 0;

    /// <summary>
    /// 操作の取り消し（1つ前の状態に戻す）
    /// </summary>
    virtual void Undo() = 0;

    /// <summary>
    /// 操作の再適用（取り消した操作をやり直す）
    /// </summary>
    virtual void Redo() = 0;

    /// <summary>
    /// コマンドの名称（デバッグ・UI表示用）
    /// </summary>
    virtual std::string GetName() const = 0;
};
