#pragma once

// すべてのシーンクラスが継承する基底インターフェース
class IScene {
public:
    // 仮想デストラクタ：派生クラスが破棄される際に正しくメモリを解放するために必須
    virtual ~IScene() = default;

    // シーンの初期化（リソース読み込みやオブジェクト生成）
    virtual void Initialize() = 0;

    // 毎フレームの更新処理（ロジック計算）
    virtual void Update() = 0;

    // 毎フレームの描画処理（描画コマンドの発行）
    virtual void Draw() = 0;

    // 毎フレームの2D/UI描画処理（ポストプロセス適用後の最終テクスチャに対する描画コマンドの発行）
    virtual void Draw2D() {}

    // ImGuiコントロールの処理
    virtual void ImGuiControl() = 0;
};
