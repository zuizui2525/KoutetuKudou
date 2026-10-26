#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

/// <summary>
/// ヘルプマニュアルのタブインデックス定数（マジックナンバー排除）
/// </summary>
namespace HelpTabIndex {
    constexpr int kShortcuts    = 0; // ショートカット一覧
    constexpr int kScene        = 1; // シーン＆ヒエラルキー
    constexpr int kPrefab       = 2; // プレハブ＆アセット管理
    constexpr int kComponents   = 3; // コンポーネント機能一覧
    constexpr int kScripting    = 4; // スクリプト開発ガイド
    constexpr int kAnimation    = 5; // アニメーション＆3D形状
    constexpr int kParticles    = 6; // エフェクト＆パーティクル
    constexpr int kArchitecture = 7; // エンジン設計＆アーキテクチャ
    constexpr int kWindows      = 8; // 全ウィンドウ操作マニュアル
}

/// <summary>
/// 検索スコアリング用定数（マジックナンバー排除）
/// 優先順位: タイトル (最優先) ＞ 見出し (中優先) ＞ 本文 (通常)
/// </summary>
namespace HelpSearchScore {
    constexpr int kTitleExact     = 1000; // タイトル完全一致
    constexpr int kTitlePartial   = 500;  // タイトル部分一致
    constexpr int kHeadingExact   = 200;  // 見出し完全一致
    constexpr int kHeadingPartial = 100;  // 見出し部分一致
    constexpr int kKeywordExact   = 80;   // キーワード完全一致
    constexpr int kKeywordPartial = 40;   // キーワード部分一致
    constexpr int kBodyExact      = 30;   // 本文完全一致
    constexpr int kBodyPartial    = 10;   // 本文部分一致
}

/// <summary>
/// マッチ箇所の種別
/// </summary>
enum class HelpMatchType {
    Title,   // タイトル一致（最上位表示）
    Heading, // 見出し一致（中位表示）
    Body     // 本文一致（通常表示）
};

/// <summary>
/// 1セクション（見出し ＋ 本文）
/// </summary>
struct HelpSection {
    std::string heading;               // 見出し（機能・小項目名）
    std::string body;                  // 本文（詳細な説明・手順・API）
    std::vector<std::string> keywords; // セクション独自のキーワード
};

/// <summary>
/// 1つのヘルプ記事（タイトル ＋ 複数の見出し・本文セクション）
/// </summary>
struct HelpArticle {
    std::string id;                    // 一意識別ID
    std::string title;                 // タイトル（大テーマ）
    std::string category;              // カテゴリ名
    int targetTabIndex;                // 所属タブインデックス (HelpTabIndex)
    std::vector<HelpSection> sections; // 見出しと本文のリスト
    std::vector<std::string> keywords; // 記事全体のキーワード
};

/// <summary>
/// 検索結果構造体
/// </summary>
struct HelpSearchResult {
    const HelpArticle* article = nullptr;
    const HelpSection* section = nullptr;
    int score = 0;
    HelpMatchType matchType = HelpMatchType::Body;
    std::string matchedSnippet; // マッチ箇所周辺の抜粋
};

/// <summary>
/// ヘルプデータベースおよび検索エンジンクラス
/// </summary>
class EngineHelpDatabase {
public:
    static constexpr size_t kSnippetContextChars = 32; // 前後抜粋文字数
    static constexpr size_t kMaxSearchResults = 20;     // 最大検索候補表示数

    /// <summary>
    /// 大文字小文字を無視した小文字化ヘルパー（ASCIIのみ安全変換、UTF-8マルチバイトは維持）
    /// </summary>
    static std::string ToLower(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    /// <summary>
    /// スニペット抜粋生成
    /// </summary>
    static std::string MakeSnippet(const std::string& text, size_t matchPos, size_t matchLen) {
        size_t startPos = (matchPos > kSnippetContextChars) ? (matchPos - kSnippetContextChars) : 0;
        size_t endPos = (std::min)(text.size(), matchPos + matchLen + kSnippetContextChars);
        std::string prefix = (startPos > 0) ? "..." : "";
        std::string suffix = (endPos < text.size()) ? "..." : "";
        return prefix + text.substr(startPos, endPos - startPos) + suffix;
    }

    /// <summary>
    /// 検索処理（タイトル ＞ 見出し ＞ 本文 の優先順位でソート）
    /// </summary>
    static std::vector<HelpSearchResult> Search(const std::string& query, bool exactMatch) {
        std::vector<HelpSearchResult> results;
        if (query.empty()) {
            return results;
        }

        const auto& articles = GetAllArticles();
        std::string lowerQuery = ToLower(query);

        for (const auto& article : articles) {
            std::string lowerTitle = ToLower(article.title);

            // 1. タイトル判定
            bool titleMatched = false;
            int bestArticleScore = 0;
            HelpMatchType bestMatchType = HelpMatchType::Body;
            const HelpSection* bestSection = nullptr;
            std::string bestSnippet;

            if (exactMatch) {
                if (article.title == query || lowerTitle == lowerQuery) {
                    bestArticleScore = HelpSearchScore::kTitleExact;
                    titleMatched = true;
                    bestMatchType = HelpMatchType::Title;
                    bestSnippet = article.title;
                }
            } else {
                size_t pos = lowerTitle.find(lowerQuery);
                if (pos != std::string::npos) {
                    bestArticleScore = HelpSearchScore::kTitlePartial;
                    titleMatched = true;
                    bestMatchType = HelpMatchType::Title;
                    bestSnippet = article.title;
                }
            }

            // 2. 記事キーワード判定
            if (!titleMatched) {
                for (const auto& kw : article.keywords) {
                    std::string lowerKw = ToLower(kw);
                    if (exactMatch) {
                        if (kw == query || lowerKw == lowerQuery) {
                            if (HelpSearchScore::kKeywordExact > bestArticleScore) {
                                bestArticleScore = HelpSearchScore::kKeywordExact;
                                bestMatchType = HelpMatchType::Title;
                                bestSnippet = "キーワード: " + kw;
                            }
                        }
                    } else {
                        if (lowerKw.find(lowerQuery) != std::string::npos) {
                            if (HelpSearchScore::kKeywordPartial > bestArticleScore) {
                                bestArticleScore = HelpSearchScore::kKeywordPartial;
                                bestMatchType = HelpMatchType::Title;
                                bestSnippet = "キーワード: " + kw;
                            }
                        }
                    }
                }
            }

            // 3. 各セクション（見出し・本文）判定
            for (const auto& section : article.sections) {
                std::string lowerHeading = ToLower(section.heading);
                std::string lowerBody = ToLower(section.body);

                // 見出しチェック
                if (exactMatch) {
                    if (section.heading == query || lowerHeading == lowerQuery) {
                        if (HelpSearchScore::kHeadingExact > bestArticleScore) {
                            bestArticleScore = HelpSearchScore::kHeadingExact;
                            bestMatchType = HelpMatchType::Heading;
                            bestSection = &section;
                            bestSnippet = section.heading;
                        }
                    }
                } else {
                    size_t pos = lowerHeading.find(lowerQuery);
                    if (pos != std::string::npos) {
                        if (HelpSearchScore::kHeadingPartial > bestArticleScore) {
                            bestArticleScore = HelpSearchScore::kHeadingPartial;
                            bestMatchType = HelpMatchType::Heading;
                            bestSection = &section;
                            bestSnippet = section.heading;
                        }
                    }
                }

                // セクションキーワードチェック
                for (const auto& kw : section.keywords) {
                    std::string lowerKw = ToLower(kw);
                    if (exactMatch) {
                        if (kw == query || lowerKw == lowerQuery) {
                            if (HelpSearchScore::kKeywordExact > bestArticleScore) {
                                bestArticleScore = HelpSearchScore::kKeywordExact;
                                bestMatchType = HelpMatchType::Heading;
                                bestSection = &section;
                                bestSnippet = section.heading + " (キーワード: " + kw + ")";
                            }
                        }
                    } else {
                        if (lowerKw.find(lowerQuery) != std::string::npos) {
                            if (HelpSearchScore::kKeywordPartial > bestArticleScore) {
                                bestArticleScore = HelpSearchScore::kKeywordPartial;
                                bestMatchType = HelpMatchType::Heading;
                                bestSection = &section;
                                bestSnippet = section.heading + " (キーワード: " + kw + ")";
                            }
                        }
                    }
                }

                // 本文チェック
                if (exactMatch) {
                    // 単語としての完全一致、または文全体一致
                    size_t pos = lowerBody.find(lowerQuery);
                    if (pos != std::string::npos && (section.body == query || lowerBody == lowerQuery)) {
                        if (HelpSearchScore::kBodyExact > bestArticleScore) {
                            bestArticleScore = HelpSearchScore::kBodyExact;
                            bestMatchType = HelpMatchType::Body;
                            bestSection = &section;
                            bestSnippet = MakeSnippet(section.body, pos, query.size());
                        }
                    }
                } else {
                    size_t pos = lowerBody.find(lowerQuery);
                    if (pos != std::string::npos) {
                        if (HelpSearchScore::kBodyPartial > bestArticleScore) {
                            bestArticleScore = HelpSearchScore::kBodyPartial;
                            bestMatchType = HelpMatchType::Body;
                            bestSection = &section;
                            bestSnippet = MakeSnippet(section.body, pos, query.size());
                        }
                    }
                }
            }

            // スコアが0より大きければ検索結果として採用
            if (bestArticleScore > 0) {
                HelpSearchResult res;
                res.article = &article;
                res.section = bestSection;
                res.score = bestArticleScore;
                res.matchType = bestMatchType;
                res.matchedSnippet = bestSnippet;
                results.push_back(res);
            }
        }

        // スコア降順ソート（タイトル ＞ 見出し ＞ 本文 の優先順位で最上部に表示）
        std::sort(results.begin(), results.end(), [](const HelpSearchResult& a, const HelpSearchResult& b) {
            return a.score > b.score;
        });

        // 最大候補数でクランプ
        if (results.size() > kMaxSearchResults) {
            results.resize(kMaxSearchResults);
        }

        return results;
    }

    /// <summary>
    /// 全ヘルプ記事データの取得（タイトル・見出し・本文の3層構造化）
    /// </summary>
    static const std::vector<HelpArticle>& GetAllArticles() {
        static const std::vector<HelpArticle> sArticles = {
            // =================================================================
            // 1. ショートカット (Shortcuts) - Tab 0
            // =================================================================
            {
                "shortcuts_general",
                "ショートカット：一般操作 (General Shortcuts)",
                "○ ショートカット一覧",
                HelpTabIndex::kShortcuts,
                {
                    { "Ctrl + S", "現在のシーンを保存 (Save Current Scene)。配置や設定をJSONファイルに書き込みます。", { "保存", "セーブ", "save" } },
                    { "Ctrl + Z", "直前の操作を元に戻す (Undo)。オブジェクトの移動、追加、削除などを取り消します。", { "戻す", "アンドゥ", "undo" } },
                    { "Ctrl + Y / Ctrl + Shift + Z", "取り消した操作をやり直す (Redo)。Undoした内容を再度適用します。", { "やり直し", "リドゥ", "redo" } },
                    { "F キー", "選択中のオブジェクトへカメラを瞬時にフォーカスします。", { "フォーカス", "カメラ", "注目", "focus" } },
                    { "F1 キー", "総合ヘルプ＆操作マニュアルウィンドウを開閉トグルします。", { "ヘルプ", "マニュアル", "help" } },
                    { "F11 キー", "フルスクリーンモードとウィンドウモードを瞬時に切り替えます。", { "全画面", "フルスクリーン", "画面サイズ" } },
                    { "Alt + F4", "アプリケーションを安全に終了します（未保存確認あり）。", { "終了", "閉じる", "exit", "quit" } }
                },
                { "キー", "ショートカット", "基本操作", "キーボード" }
            },
            {
                "shortcuts_camera",
                "ショートカット：シーン・カメラ操作 (Camera Shortcuts)",
                "○ ショートカット一覧",
                HelpTabIndex::kShortcuts,
                {
                    { "右ドラッグ (Right Drag)", "カメラの視線回転（Look Around / FPS視点回転）。視点を自由に見渡します。", { "視線", "見渡す", "回転", "FPS" } },
                    { "右ドラッグ + W / S", "カメラの前後移動。Wで前進、Sで後退します。", { "前進", "後退", "移動" } },
                    { "右ドラッグ + A / D", "カメラの水平移動。Aで左へ平行移動、Dで右へ平行移動します。", { "左右", "平行移動" } },
                    { "右ドラッグ + E / Q", "カメラの垂直移動。Eで上昇、Qで下降します。", { "上昇", "下降", "上下" } },
                    { "マウスホイール回転", "前後ズームイン / ズームアウト (Zoom In / Out)。", { "ズーム", "拡大", "縮小" } },
                    { "中クリックドラッグ", "カメラの画面平行移動 (Pan)。現在向いている方向を保ったまま視点を移動します。", { "パン", "平行移動", "中クリック" } }
                },
                { "カメラ", "操作", "視点", "マウス", "FPS", "移動" }
            },
            {
                "shortcuts_gizmo",
                "ショートカット：ギズモ・操作 (Gizmo Manipulation)",
                "○ ショートカット一覧",
                HelpTabIndex::kShortcuts,
                {
                    { "ヒエラルキー 左クリック", "オブジェクトを選択し、平行移動ギズモ (Translate) を有効化します。", { "移動", "Translate", "ギズモ" } },
                    { "ヒエラルキー 中クリック", "オブジェクトを選択し、回転ギズモ (Rotate) を有効化します。", { "回転", "Rotate" } },
                    { "ヒエラルキー 右クリック", "オブジェクトを選択し、拡縮ギズモ (Scale) を有効化します。", { "拡縮", "スケール", "Scale" } },
                    { "ヒエラルキー ダブルクリック", "オブジェクトを選択してカメラを瞬時にフォーカス (F) します。", { "フォーカス", "ダブルクリック" } },
                    { "ギズモ軸ドラッグ", "選択中の3DギズモのXYZ軸をマウスでドラッグしてTransformを直接操作します（Undo/Redo対応）。", { "ドラッグ", "Undo" } },
                    { "ヒエラルキー [x] ボタン", "対象のGameObjectを即座に安全削除します（Undo/Redo対応）。", { "削除", "消去", "delete" } }
                },
                { "ギズモ", "操作", "Transform", "移動", "回転", "拡縮" }
            },

            // =================================================================
            // 2. シーン＆ヒエラルキー (Scene & Hierarchy) - Tab 1
            // =================================================================
            {
                "scene_management",
                "シーン管理機能 (Scene Management)",
                "○ シーン＆ヒエラルキー",
                HelpTabIndex::kScene,
                {
                    { "シーン切り替え", "メニューバー「シーン」内の保存済みシーン (.json) をクリックして即座に遷移・ロードします。", { "切り替え", "遷移", "ロード" } },
                    { "新規シーン作成 (New Scene)", "「New Scene」入力欄に名前を入力して Enter またはボタンクリックで空の新規シーンを生成します。", { "新規作成", "新しいシーン", "New Scene" } },
                    { "シーン保存 (Save Scene)", "Ctrl+S またはメニューから実行し、シーン内の全オブジェクト、Transform、コンポーネントを保存します。", { "保存", "セーブ" } },
                    { "シーン削除 (Delete Scene)", "誤操作を防ぐ安全確認モーダルが表示され、承認後にファイルを完全削除します。編集中シーンの場合は自動フォールバックします。", { "削除", "削除確認" } },
                    { "画面切り替え演出 (Scene Transition)", "シーン一覧で各シーンを右クリックして入場(IN:シアン)/退場(OUT:オレンジ)のトランジションフェード演出を設定可能です。", { "トランジション", "フェード", "演出", "IN", "OUT" } }
                },
                { "シーン", "Scene", "管理", "遷移", "保存" }
            },
            {
                "hierarchy_creation",
                "ゲームオブジェクト生成＆ヒエラルキー操作 (Hierarchy & Creation)",
                "○ シーン＆ヒエラルキー",
                HelpTabIndex::kScene,
                {
                    { "新規オブジェクト生成手順", "ヒエラルキーの余白部分を【右クリック】→【+ Create GameObject】から種類を選択して即時生成します。", { "生成", "新規作成", "右クリック" } },
                    { "+ Create Empty", "コンポーネントを持たない空のGameObjectを作成。フォルダ分けや親ノード、位置管理に便利です。", { "空", "Empty", "親" } },
                    { "+ 3D Mesh (Cube等)", "MeshRendererコンポーネント付きの3D形状を作成。デフォルトはCubeでSphereやCylinder等に変更可能。", { "3D", "メッシュ", "Cube", "Mesh" } },
                    { "+ 2D Sprite", "SpriteRendererコンポーネント付きの2D画像オブジェクトを作成。UIや2Dキャラクターに最適です。", { "2D", "スプライト", "Sprite", "画像" } },
                    { "+ Camera", "Cameraコンポーネント付きのカメラを作成。メインカメラやサブビュー、カットシーン用カメラに利用します。", { "カメラ", "Camera" } },
                    { "+ Light", "LightComponent付きの光源を作成。DirectionalLight（太陽光）やPointLight（松明・ランプ）を設定できます。", { "ライト", "光源", "照明", "Light" } },
                    { "+ Text 3D / 2D", "TextRendererコンポーネント付きの文字オブジェクトを作成。頭上ネームや案内看板を3D空間に配置できます。", { "テキスト", "文字", "Text" } },
                    { "検索フィルター & ドラッグ＆ドロップ", "ヒエラルキー上部のテキスト入力で名前絞り込みが可能。項目をドラッグしてインスペクターの参照スロットへ直接ドロップできます。", { "検索", "絞り込み", "ドラッグ", "参照" } }
                },
                { "ヒエラルキー", "GameObject", "作成", "ツリー", "Hierarchy" }
            },

            // =================================================================
            // 3. プレハブ＆アセット管理 (Prefab & Assets) - Tab 2
            // =================================================================
            {
                "prefab_system",
                "プレハブシステム (Prefab System)",
                "○ プレハブ＆アセット管理",
                HelpTabIndex::kPrefab,
                {
                    { "Save as Prefab (プレハブ保存)", "ヒエラルキーでオブジェクトを選択し、インスペクターの「Save as Prefab」ボタンを押すと resources/Prefabs/{名前}.prefab に一発保存されます。", { "保存", "テンプレート", "プレハブ化" } },
                    { "DND実体化配置", "コンテンツブラウザ内の .prefab ファイルをシーンビューまたはヒエラルキーへドラッグ＆ドロップすると、その場に実体化配置されます。", { "配置", "DND", "ドラッグ＆ドロップ" } },
                    { "スクリプトからの動的生成 (Instantiate)", "ScriptComponent::Instantiate(\"Prefab名\", pos, rot); を呼ぶことで、弾や敵キャラクターを実行中に動的生成できます。", { "動的生成", "Instantiate", "出現" } },
                    { "動的生成オブジェクトの破棄 (Destroy)", "ScriptComponent::Destroy(gameObject); を呼ぶことで、寿命を迎えた弾や倒された敵を安全にシーンから破棄します。", { "破棄", "Destroy", "削除" } }
                },
                { "プレハブ", "Prefab", "テンプレート", "Instantiate", "Destroy" }
            },
            {
                "content_browser",
                "コンテンツブラウザ (Content Browser)",
                "○ プレハブ＆アセット管理",
                HelpTabIndex::kPrefab,
                {
                    { "ディレクトリ切り替え", "上部の「resources」と「Scripts」ボタンで、リソースフォルダとスクリプトフォルダをワンクリックで切り替えられます。", { "リソース", "スクリプト", "フォルダ" } },
                    { "履歴ナビゲーション", "「<」「>」ボタンでフォルダの閲覧履歴を行き来できます。アドレスバーのパスをクリックして上の階層に戻ることも可能です。", { "履歴", "戻る", "進む" } },
                    { "アセットアイコン認識", "テクスチャ(.png/.jpg)、3Dモデル(.obj/.gltf)、音声(.wav/.mp3)、シーン(.json)、スクリプト(.cpp/.h)、プレハブ(.prefab)をアイコン付きで視覚的に識別・管理できます。", { "アセット", "アイコン", "テクスチャ", "モデル" } },
                    { "スクリプトのDNDアタッチ", "コンテンツブラウザ内のスクリプト (.cpp) をインスペクターへドラッグ＆ドロップすると、選択中オブジェクトへ直接アタッチできます。", { "アタッチ", "スクリプトDND" } }
                },
                { "コンテンツブラウザ", "ファイル", "アセット", "Browser" }
            },

            // =================================================================
            // 4. コンポーネント機能＆インスペクター内部変数一覧 (Components) - Tab 3
            // =================================================================
            {
                "comp_transform",
                "Transform＆オブジェクト共通設定 (Transform & Common)",
                "○ コンポーネント機能一覧",
                HelpTabIndex::kComponents,
                {
                    { "Position (translate)", "オブジェクトのワールド空間での位置座標 (X, Y, Z)。2Dオブジェクトはピクセル座標系（画面中央: 640, 360）、3Dオブジェクトは原点 (0, 0, 0) 基準で配置します。", { "Position", "translate", "座標", "位置", "画面中央" } },
                    { "Rotation (rotate)", "各軸まわりの回転角度（度数法/Degree）。X軸(ピッチ)、Y軸(ヨー)、Z軸(ロール)でオブジェクトの向きを決定します。", { "Rotation", "rotate", "角度", "向き", "回転" } },
                    { "Scale (scale)", "各軸の拡大縮小倍率 (X, Y, Z)。1.0 が原寸大（等倍）です。負の値を指定すると反転します。", { "Scale", "scale", "拡大", "縮小", "サイズ", "倍率" } },
                    { "2D画面中央 / 3D原点リセットヘルパー", "3Dオブジェクトが誤って2D座標にある場合などに、黄色警告テキストとワンクリック復帰ボタンが表示されます。", { "座標補正", "原点リセット", "画面中央配置" } },
                    { "Active チェックボックス", "オブジェクト全体の有効 / 無効を切り替えます。無効化すると子の描画や更新処理も一括で停止します。", { "Active", "有効", "無効", "非表示" } }
                },
                { "Transform", "位置", "回転", "拡大", "座標", "Position", "Rotation", "Scale" }
            },
            {
                "comp_mesh_renderer",
                "MeshRenderer コンポーネント (3Dメッシュ描画・インスペクター変数)",
                "○ コンポーネント機能一覧",
                HelpTabIndex::kComponents,
                {
                    { "Mesh Type (メッシュタイプ)", "描画する3D形状をコンボボックスで選択。Cube, Sphere, Cylinder, Cone, Pyramid, TriangularPyramid, Hemisphere, Ring, FlatRing, Square/Plane, Custom Model から切り替えます。", { "Mesh Type", "形状", "プリミティブ", "メッシュタイプ", "Cube", "Sphere" } },
                    { "Model Path / Key (modelKey_)", "MeshTypeがCustom Modelの場合に表示。読み込む外部モデル (.obj / .gltf) のアセットパス。コンテンツブラウザからのドラッグ＆ドロップ受け取りに対応。", { "Model Key", "Model Path", "モデル", "gltf", "obj", "DND" } },
                    { "Texture Path / Key (textureKey_)", "モデル表面にマッピングするテクスチャ画像 (.png) のパス。コンテンツブラウザからのドラッグ＆ドロップ受け取りに対応。", { "Texture Key", "Texture Path", "テクスチャ", "画像", "PNG" } },
                    { "Material Color (color_)", "マテリアルのベース乗算カラー (RGBA)。カラーピッカーで色味やアルファ（半透明度）を変更可能。", { "Material Color", "color", "マテリアル色", "色", "透明度", "アルファ" } },
                    { "Receive Lighting (receiveLighting_)", "シーン内のディレクショナルライトやポイントライトの光（陰影計算）を受けるかどうかのトグルチェックボックス。", { "Receive Lighting", "ライティング", "陰影", "受光", "光" } },
                    { "Show Skeleton & Animation Timeline", "アニメーション付きモデルの場合に表示。ボーン骨組みのリアルタイム可視化トグル、再生/一時停止/停止ボタン、ループ再生トグル、タイムラインシークバー。", { "Show Skeleton", "Timeline", "Loop", "アニメーション", "ボーン", "タイムライン" } },
                    { "Offset Transform (個別配置)", "GameObjectのTransformとは独立した、メッシュ固有の相対位置・回転・拡縮オフセットを設定可能です。", { "Offset Transform", "相対配置", "個別位置" } }
                },
                { "MeshRenderer", "メッシュ", "3D描画", "マテリアル", "テクスチャ", "ライティング", "モデル" }
            },
            {
                "comp_sprite_renderer",
                "SpriteRenderer コンポーネント (2Dスプライト描画・インスペクター変数)",
                "○ コンポーネント機能一覧",
                HelpTabIndex::kComponents,
                {
                    { "Texture Path / Key (textureKey_)", "2D画像として描画するテクスチャファイル (.png) のパス。コンテンツブラウザから直接ドラッグ＆ドロップ可能。", { "Texture Key", "スプライト画像", "テクスチャ", "png" } },
                    { "Size (size_)", "スプライトの描画サイズ (幅 Width, 高さ Height / pixels)。等倍または任意のアスペクト比に拡大縮小可能。", { "Size", "Width", "Height", "幅", "高さ", "ピクセルサイズ" } },
                    { "Anchor Point (anchorPoint_)", "スプライトの描画・回転・拡縮の基準点 (0.0〜1.0)。(0.5, 0.5) で画像中心、(0.0, 0.0) で左上基準。", { "Anchor Point", "アンカーポイント", "原点", "基準点" } },
                    { "Color / Alpha (color_)", "スプライトの乗算カラーおよびフェード透明度 (RGBA)。カラーピッカーで調整します。", { "Color", "Alpha", "透明度", "アルファ", "色" } },
                    { "Flip X / Flip Y (反転)", "スプライト画像を水平方向（左右）または垂直方向（上下）に反転描画するチェックボックス。", { "Flip X", "Flip Y", "反転", "左右反転", "上下反転" } }
                },
                { "SpriteRenderer", "2Dスプライト", "スプライト", "画像描画", "Anchor", "Flip", "Size" }
            },
            {
                "comp_colliders",
                "BoxCollider & SphereCollider コンポーネント (当たり判定・インスペクター変数)",
                "○ コンポーネント機能一覧",
                HelpTabIndex::kComponents,
                {
                    { "Center (centerOffset_)", "オブジェクト原点からのコライダー中心オフセット座標 (X, Y, Z)。当たり判定の位置をオブジェクトの中心から微調整します。", { "Center", "centerOffset", "中心", "オフセット", "当たり判定中心" } },
                    { "Size (size_) [BoxCollider]", "直方体コライダーの各軸サイズ (幅X, 高さY, 奥行きZ)。バウンディングボックスの体積を決定します。", { "Size", "直方体サイズ", "コライダーサイズ", "大きさ" } },
                    { "Radius (radius_) [SphereCollider]", "球体コライダーの球半径。全方位に均等な当たり判定を形成します。", { "Radius", "半径", "球半径", "球体コライダー" } },
                    { "Is Trigger (isTrigger_)", "物理的な衝突押し出しを行うか、すり抜けイベント検知（トリガー判定）として扱うかの切り替えチェックボックス。", { "Is Trigger", "トリガー", "すり抜け", "衝突検知" } },
                    { "Show Gizmo & Gizmo Thickness", "SceneView 上でコライダーのワイヤーフレーム（緑・黄）を表示するトグル、およびデバッグ線の太さ調整スライダー。", { "Show Gizmo", "Gizmo Thickness", "ワイヤーフレーム", "ギズモ", "デバッグ表示" } }
                },
                { "Collider", "コライダー", "当たり判定", "BoxCollider", "SphereCollider", "Center", "Size", "Radius", "Trigger" }
            },
            {
                "comp_lights_camera",
                "LightComponent & CameraComponent (照明・カメラ・インスペクター変数)",
                "○ コンポーネント機能一覧",
                HelpTabIndex::kComponents,
                {
                    { "Light Type (光源の種類)", "Directional（太陽光等の無限遠平行光源）または Point（松明・電球等の点光源）の切り替えコンボ。", { "Light Type", "Directional", "Point", "光源", "ライト" } },
                    { "Light Color & Intensity (光の色と強さ)", "光源が放つ光のRGBカラー、および明るさの強度 (Intensity)。スライダーで調整します。", { "Intensity", "Color", "照度", "明るさ", "光の強さ" } },
                    { "Radius & Decay [PointLight]", "ポイントライトの光が届く最大影響半径 (Radius) と、距離に応じた光の減衰率 (Decay)。", { "Radius", "Decay", "減衰", "光の届く距離", "半径" } },
                    { "Fov (Field of View) [Camera]", "カメラの垂直視野角（画角 / Degree）。通常は45°〜60°、広角は70°〜90°、望遠は20°〜35°を設定します。", { "Fov", "Field of View", "視野角", "画角", "カメラ視野" } },
                    { "Near Clip & Far Clip [Camera]", "カメラが描画する最前面限界距離 (Near) と最遠限界距離 (Far)。0.1〜1000.0等の範囲を指定します。", { "Near Clip", "Far Clip", "クリップ距離", "描画限界" } },
                    { "Clear Color & Is Main Camera [Camera]", "背景の塗りつぶし色 (Clear Color)、およびゲーム実行画面 (GameView) の主視点として採用するトグル。", { "Clear Color", "Is Main Camera", "メインカメラ", "背景色" } }
                },
                { "Light", "Camera", "ライト", "カメラ", "Fov", "Intensity", "Radius", "Clip" }
            },
            {
                "comp_audio_text_effects",
                "AudioSource & TextRenderer & 演出系コンポーネント (インスペクター変数)",
                "○ コンポーネント機能一覧",
                HelpTabIndex::kComponents,
                {
                    { "Sound Key / Volume / Pitch / Loop [AudioSource]", "再生音声ファイルキー、音量(Volume: 0.0〜1.0)、音程・速度(Pitch: 0.5〜2.0)、ループ再生トグル(Is Loop)、即座テスト再生ボタン。", { "Sound Key", "Volume", "Pitch", "Loop", "音量", "ピッチ", "ループ", "効果音", "BGM" } },
                    { "Text / Font Size / Billboard [TextRenderer]", "描画文字列テキスト(Text)、フォントサイズ(Font Size)、文字色(Color)、3D空間で常にカメラ正面を向くビルボード回転トグル(Billboard)。", { "Text", "Font Size", "Billboard", "文字", "フォント", "看板", "ビルボード" } },
                    { "Rotation Speed [Rotator]", "オブジェクトを毎フレーム自動回転させる各軸まわりの回転速度 (deg/sec)。X/Y/Z軸ごとに独立設定可能。", { "Rotator", "Rotation Speed", "自動回転", "回転速度" } },
                    { "Axis / Amplitude / Frequency [SinOscillator]", "サイン波に基づく往復浮遊運動。運動軸(Axis: X/Y/Z)、揺れ幅振幅(Amplitude)、振動スピード周波数(Frequency)。", { "SinOscillator", "Axis", "Amplitude", "Frequency", "浮遊", "往復", "振幅", "周波数" } },
                    { "Blink Speed / Min Alpha / Max Alpha [Blink]", "アルファ明滅演出。点滅スピード(Blink Speed)、最小アルファ値(Min Alpha)、最大アルファ値(Max Alpha)。", { "Blink", "Blink Speed", "Min Alpha", "Max Alpha", "点滅", "明滅" } },
                    { "Target Distance / Orbit Speed / Elevation Angle [CameraOrbit]", "対象を周回する自動旋回カメラ。維持距離(Target Distance)、旋回スピード(Orbit Speed)、見下ろし角度(Elevation Angle)。", { "CameraOrbit", "Orbit Speed", "Target Distance", "Elevation Angle", "旋回", "周回カメラ" } }
                },
                { "AudioSource", "TextRenderer", "Rotator", "SinOscillator", "Blink", "CameraOrbit", "Volume", "Pitch", "Text", "Amplitude" }
            },

            // =================================================================
            // 5. スクリプト開発ガイド (Scripting Guide) - Tab 4
            // =================================================================
            {
                "scripting_guide",
                "スクリプト開発ガイド＆標準API (Scripting Guide)",
                "○ スクリプト開発ガイド",
                HelpTabIndex::kScripting,
                {
                    { "スクリプト新規作成手順", "インスペクターの「+ Add Component」→「+ New Script...」から名前を入力すると、Scripts/ 内に丁寧なコメント入りの .h と .cpp が自動生成され即アタッチされます。", { "新規スクリプト", "自動生成", "New Script" } },
                    { "ライフサイクル関数", "Awake()(生成時1度) → Start()(初回更新前) → OnUpdate()(毎フレーム) → OnInspectorGUI()(GUI編集) → OnGUI()(HUD描画) → OnDestroy()(破棄時)。", { "ライフサイクル", "Awake", "Start", "OnUpdate", "OnGUI" } },
                    { "標準便利API", "GetOwner()(親GameObject), targetObject_(外部参照), Instantiate()(動的生成), Destroy()(安全破棄), ShakeCamera()(画面揺れ演出)。", { "API", "GetOwner", "ShakeCamera", "Instantiate" } },
                    { "REGISTER_SCRIPT マクロ", "生成された .cpp の上部にある REGISTER_SCRIPT(クラス名) は必須です。エディタがスクリプトを自動認識するために削除しないでください。", { "マクロ", "REGISTER_SCRIPT", "リフレクション" } },
                    { "マジックナンバーの排除", "コード内に直接数値を埋め込まず、メンバ変数（moveSpeed_等）や定数にし、OnInspectorGUI() でスライダー調整可能にすることを推奨します。", { "マジックナンバー", "定数", "変数" } }
                },
                { "スクリプト", "Script", "C++", "開発", "API", "ライフサイクル" }
            },

            // =================================================================
            // 6. アニメーション＆3D形状 (Animation & 3D Shapes) - Tab 5
            // =================================================================
            {
                "animation_guide",
                "アニメーション機能＆GPUスキニング (Animation & GPU Skinning)",
                "○ アニメーション＆3D形状",
                HelpTabIndex::kAnimation,
                {
                    { "対応フォーマットと配置", "Assimp対応の .gltf / .glb（推奨）および .obj をサポート。resources/ 直下に配置します。", { "gltf", "glb", "obj", "モデル" } },
                    { "アニメーション再生設定手順", "MeshRenderer の Mesh Type を「10: Custom Model」にし、Model Key と Animation Key に .gltf パスを入力すると専用パネルが出現します。", { "アニメーション再生", "Custom Model", "手順" } },
                    { "タイムラインとループ再生", "再生/一時停止/停止ボタン、ループトグル、Timelineシークバーにより任意フレームへ瞬時に移動できます。", { "再生", "一時停止", "ループ", "タイムライン" } },
                    { "Show Skeleton (ボーン可視化)", "チェックをONにすると、骨組みライン（関節の親子階層）がリアルタイム可視化され、ポーズの確認が容易になります。", { "ボーン", "スケルトン", "骨組み", "可視化" } },
                    { "GPU スキニング", "ボーンの動きに合わせて頂点シェーダで高速に変形計算され、滑らかなキャラクターアニメーションを実現します。", { "スキニング", "GPUスキニング", "変形" } }
                },
                { "アニメーション", "Animation", "ボーン", "骨", "gltf", "スキニング" }
            },
            {
                "primitive_meshes",
                "3D幾何学形状一覧と用途 (3D Primitive Meshes)",
                "○ アニメーション＆3D形状",
                HelpTabIndex::kAnimation,
                {
                    { "0: Cube (立方体)", "地面、壁、足場、障害物、木箱、ブロック建築など汎用的な直方体オブジェクトに最適です。", { "Cube", "立方体", "箱" } },
                    { "1: Sphere (球体)", "弾丸、オーブ、ボール、惑星、全方位の当たり判定や魔法玉に最適です。", { "Sphere", "球", "玉" } },
                    { "2: Cylinder (円柱)", "柱、支柱、パイプ、ビーム照射エフェクト、筒状のギミックやバリアに最適です。", { "Cylinder", "円柱", "柱" } },
                    { "3: Cone (円錐)", "スパイク（トゲ）、スポットライトコーン、マーカー、矢印の先端などに最適です。", { "Cone", "円錐", "トゲ" } },
                    { "4: Pyramid (四角錐) / 5: TriangularPyramid (三角錐)", "ピラミッド型オブジェクト、四面体結晶、鋭利なクリスタル、浮遊鉱石に最適です。", { "Pyramid", "ピラミッド", "四角錐", "三角錐", "クリスタル" } },
                    { "6: Hemisphere (半球)", "ドーム状バリア、シェルター、丘陵、着弾時の衝撃波半球エフェクトに最適です。", { "Hemisphere", "半球", "ドーム", "バリア" } },
                    { "7: Ring (立体リング) / 8: FlatRing (平面リング)", "トーラス型ワープゲート、足元の魔法陣、着弾エフェクトの波紋、サークルマーカーに最適です。", { "Ring", "リング", "魔法陣", "波紋" } },
                    { "9: Square / Plane (平面板)", "2D看板、水面、影デカール、ガラス板、UI背景などに最適です。", { "Plane", "平面", "板", "地面" } },
                    { "10: Custom Model (外部モデル)", "Assimp対応の .obj / .gltf 外部モデル。キャラクターや武器に使用します。", { "モデル", "Custom Model" } }
                },
                { "プリミティブ", "形状", "メッシュ", "3D形状", "Cube", "Sphere" }
            },

            // =================================================================
            // 7. エフェクト＆パーティクル (Effects & Particles) - Tab 6
            // =================================================================
            {
                "particle_studio",
                "GPUパーティクルスタジオの使い方 (ParticleStudioWindow)",
                "○ エフェクト＆パーティクル",
                HelpTabIndex::kParticles,
                {
                    { "ワークスペース切り替えと専用画面の起動", "エディタ上部のワークスペースバーから「Studio」タブをクリックすると、パーティクル制作に特化した専用レイアウトが全画面で展開されます。", { "ワークスペース", "Studio", "スタジオ", "起動" } },
                    { "オフスクリーンプレビューのカメラ操作", "独立した専用プレビュー画面上で、右ドラッグ(視線オービット回転)、中ドラッグ(画面平行パン)、ホイール(ズーム)が可能です。", { "プレビュー", "オービット", "カメラ操作" } },
                    { "エミッター放射形状 (Emitter Shape)", "Sphere(球状全方位爆散)、Cone(扇状・火炎放射コーン)、Box(直方体空間散布)、Circle(平面リング拡散)の各形状から選択可能です。", { "エミッター", "放射形状", "Sphere", "Cone", "Box", "Circle" } },
                    { "物理＆挙動パラメータ設定", "粒子発生数(Count)、寿命(LifeTime)、初速(Velocity)、重力(Gravity)、空気抵抗(Drag)、拡散角度を直感的なスライダーで微調整できます。", { "重力", "初速", "寿命", "速度", "物理" } },
                    { "カラーグラデーションとサイズ変化", "発生時の開始カラーから消滅時の終了カラーへの滑らかな線形補間、および寿命に合わせたスケール拡縮変化を設定可能です。", { "カラー", "色", "グラデーション", "スケール", "透明度" } },
                    { "JSONプリセットの保存とロード", "作成したパーティクルパラメータは resources/Config/Particles/ に名前を付けてJSON形式でワンクリック保存・読み込みが可能です。", { "プリセット", "保存", "ロード", "JSON", "Config" } }
                },
                { "パーティクルスタジオ", "GPUパーティクル", "エフェクト制作", "ParticleStudio", "炎", "煙", "爆発" }
            },
            {
                "particle_component_usage",
                "パーティクルコンポーネント運用ガイド (ParticleEffectComponent)",
                "○ エフェクト＆パーティクル",
                HelpTabIndex::kParticles,
                {
                    { "GameObjectへのアタッチ", "シーン内のGameObjectを選択し、インスペクターの「+ Add Component」から「ParticleEffect」を追加します。", { "アタッチ", "追加", "Component" } },
                    { "設定項目 (EffectName / Offset / Loop / AutoPlay)", "EffectName: 再生するエフェクト名。Offset: 親からの相対座標。Loop: 継続再生(松明・煙等)。AutoPlay: シーンロード時の自動再生トリガー。", { "設定", "Loop", "AutoPlay", "Offset", "ループ" } },
                    { "インスペクターからのテスト再生", "「Play ▶」ボタンで即座にエフェクトが実体化再生され、「Stop ⏹」で安全に停止・消去されます。", { "テスト再生", "Play", "Stop", "停止" } },
                    { "C++ スクリプトからの再生制御コード", "auto fx = GetComponent<ParticleEffectComponent>(); if (fx) { fx->Play(); } のように攻撃ヒット時や着地時に呼び出せます。", { "スクリプト", "C++", "呼び出し", "コード例" } }
                },
                { "ParticleEffect", "コンポーネント", "アタッチ", "エフェクト再生" }
            },
            {
                "effect_manager_api",
                "エフェクトマネージャ API リファレンス (EffectManager)",
                "○ エフェクト＆パーティクル",
                HelpTabIndex::kParticles,
                {
                    { "2D/3Dエフェクトの一発再生 API", "EffectManager::GetInstance()->PlayEffect3D(\"Hit\", worldPos); でシーン内の任意3D座標にヒット演出を即時発生させます。", { "PlayEffect3D", "PlayEffect2D", "Hit", "一発再生" } },
                    { "超高速大量 GPU パーティクル再生 API", "EffectManager::GetInstance()->PlayGPUEffect(pos, 1000); により、Compute Shaderを用いた数千〜数万粒子の圧倒的エフェクトを低負荷で発生させます。", { "PlayGPUEffect", "大量粒子", "ComputeShader", "超高速" } },
                    { "登録済みエフェクト一覧ウィンドウ (Effect List)", "メニュー「ウィンドウ」→「エフェクト一覧」から、登録されている全エフェクト設定のパラメータ確認とテスト再生が可能です。", { "エフェクト一覧", "EffectList", "確認" } },
                    { "プリセット設定 (Fireworks / Ripple)", "花火の炸裂(FireworksBurst)、火炎(FireworksFlame)、火花(FireworksSparkle)、波紋(Ripple)などのプリセット設定が標準提供されています。", { "花火", "Fireworks", "Ripple", "波紋", "プリセット" } }
                },
                { "EffectManager", "API", "GPUパーティクル", "マネージャ", "コード" }
            },

            // =================================================================
            // 8. エンジン設計＆アーキテクチャ (Engine Architecture) - Tab 7
            // =================================================================
            {
                "engine_architecture",
                "ZuizuiEngine アーキテクチャ＆ゲームループ設計",
                "○ エンジン設計＆構造",
                HelpTabIndex::kArchitecture,
                {
                    { "DirectX 12 高速レンダリング基盤", "ID3D12Device, CommandQueue, SwapChain, DescriptorHeapをDxCommonで一元管理。PSOManagerによりシェーダーとパイプラインを最適化キャッシュします。", { "DirectX12", "DxCommon", "PSO", "グラフィックス基盤" } },
                    { "フレーム実行順序（メインゲームループ）", "ProcessMessage() → BeginFrame() → Update()(Input/Scene/GameObject/Physics/Script) → ImGui/Editor → Render() → Present() → EndFrame() の確定順序で実行されます。", { "ゲームループ", "フレーム順序", "Update", "Render", "Present" } },
                    { "シーンシリアライズとJSON完全永続化", "SceneSerializer により、GameObjectの親子階層、Transform、コンポーネント固有設定、UUID参照関係がすべて安全にJSON保存・自動復元されます。", { "シリアライズ", "JSON", "永続化", "復元" } },
                    { "コンポーネント指向＆動的リフレクション", "すべての機能は IComponent を継承。REGISTER_SCRIPT マクロにより型情報が自動登録され、エディタ上での動的追加・GUI描画が実現されています。", { "コンポーネント指向", "リフレクション", "IComponent" } }
                },
                { "アーキテクチャ", "エンジン設計", "DirectX12", "ゲームループ", "仕組み" }
            },

            // =================================================================
            // 9. 全ウィンドウ操作マニュアル (Windows & Tools) - Tab 8
            // =================================================================
            {
                "window_manual_main",
                "全ウィンドウ＆エディタツール操作マニュアル",
                "○ 全ウィンドウ操作マニュアル",
                HelpTabIndex::kWindows,
                {
                    { "SceneView (シーンビュー)", "エディタ用3D空間ビューポート。FPS風カメラ操作、3Dギズモ（移動/回転/拡大）、Fキーフォーカス、グリッドやコライダーワイヤーフレームを描画します。", { "SceneView", "シーンビュー", "3Dビュー" } },
                    { "GameView (ゲームビュー)", "実際のプレイヤー視点画面。アスペクト比（16:9固定やフルサイズ）、解像度スケール、実行中のフルスクリーン表示に対応します。", { "GameView", "ゲームビュー", "実機画面" } },
                    { "Hierarchy (ヒエラルキー)", "シーン内の全GameObjectツリー。右クリック作成メニュー、DND親子付け、削除[x]（Undo対応）、名前リアルタイム検索バーを提供します。", { "Hierarchy", "ヒエラルキー", "ツリー" } },
                    { "Inspector (インスペクター)", "選択中オブジェクトのTransform、コンポーネント一覧、スクリプト公開変数、2D/3D座標補正ボタン、プレハブ保存ボタンを統合表示します。", { "Inspector", "インスペクター", "プロパティ" } },
                    { "ContentBrowser (コンテンツブラウザ)", "resources と Scripts のフォルダ階層ブラウザ。アイコン表示、履歴戻る/進む、ファイルのドラッグ＆ドロップアタッチに対応します。", { "ContentBrowser", "コンテンツブラウザ", "ブラウザ" } },
                    { "DebugConsole (デバッグコンソール)", "エンジンのログメッセージをリアルタイム表示。Info, Warning, Errorのレベル別トグルフィルタ、ログクリア機能を提供します。", { "Console", "コンソール", "ログ", "デバッグ" } },
                    { "PerformanceMonitor (パフォーマンスモニター)", "リアルタイムFPS、フレーム時間（ms）の推移グラフ、CPU/GPU処理時間、メモリ使用量を視覚的に監視します。", { "PerfMonitor", "パフォーマンス", "FPS", "負荷" } },
                    { "PostProcessWindow (ポストプロセス)", "ブルーム（光あふれ）、トーンマッピング、ヴィネット（画面端の暗転）、被写界深度などの画面全体エフェクトをリアルタイム調整します。", { "PostProcess", "ポストプロセス", "ブルーム", "ヴィネット" } },
                    { "Workspaces (ワークスペース切り替え)", "Layout（標準エディタ配置）と Studio（パーティクル制作専用レイアウト）を画面上部のタブからワンクリックで切り替えます。", { "Workspace", "ワークスペース", "画面配置" } }
                },
                { "ウィンドウ", "マニュアル", "ツール", "使い方", "画面" }
            }
        };

        return sArticles;
    }
};
