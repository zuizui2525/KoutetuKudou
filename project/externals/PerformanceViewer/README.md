# PerformanceViewer - パフォーマンス解析ビューワー (独自ゲームエンジン統合ガイド)

このツールは、ゲーム実行時のパフォーマンス（FPS、CPUメモリ使用量、GPU VRAM使用量）を計測・記録し、Webブラウザ上でリアルタイム監視や、高負荷スパイク検出時の直前3秒間のインスタント・リプレイ確認ができる開発者向けデバッグツールです。

本ツールはスタンドアロンで動作し、他の開発者と計測データを共有したり、独自のゲームエンジンに統合して活用できるように設計されています。

---

## 📋 目次
- [📂 フォルダ構成（開発用 vs 配布用）](#フォルダ構成開発用-vs-配布用)
- [🛠️ ゲームプロジェクトへの組み込み方法](#ゲームプロジェクトへの組み込み方法)
- [🔌 ゲームとビューワーの通信 API 仕様 (独自エンジン向け)](#ゲームとビューワーの通信-api-仕様-独自エンジン向け)
- [🚀 使用方法](#使用方法)
- [⚙️ 各種モードの説明](#各種モードの説明)
- [🛑 終了方法および多重起動時の挙動](#終了方法および多重起動時の挙動)
- [🛠️ 高度な設定：system_log.json のデータ仕様](#高度な設定system_logjson-のデータ仕様)

---

## 📂 フォルダ構成（開発用 vs 配布用）

本ツールのパッケージは、配布目的や役割に応じて中身を整理することができます。

### 1. 配布用アセット構成 (テスターや一般開発者に送る場合)
お友達やチームメンバーに「計測データの確認」や「ツールの利用」だけを行ってほしい場合は、ソースコードは不要です。以下の **5つのファイル・フォルダのみ** を含めて Zip 等で配布してください。非常に軽量（数MB）で動作します。

* `PerformanceViewer.exe` : ツール本体（C++製のWeb/WebSocketサーバー）
* `index.html` : ビューワーの画面レイアウト（HTML5）
* `style.css` : ビューワーのデザイン（CSS3 / ダークテーマ / サイバー調）
* `app.js` : データフェッチおよびグラフ描画、動画再生制御（JavaScript）
* `README.md` : この取扱説明書
* `out/` : 計測データ（動画やログ）の一元管理用フォルダ
  * *過去の計測データも含めて共有したい場合*: この `out/` フォルダの中にレポートを含めたまま共有します。
  * *クリーンな状態で共有したい場合*: この `out/` フォルダを削除、または中身を空にして共有します（起動時に自動生成されます）。

### 2. 開発用ファイル構成 (コードの変更やビルドを行いたい場合)
ビューワー本体（C++）のコードを変更してデバッグ・ビルドし直す場合は、上記に加えて以下のファイルを含めて共有します。

* `main.cpp` : ビューワーサーバー本体のC++ソースコード
* `PerformanceViewer.vcxproj` : MSBuild 用プロジェクトファイル
* `build_viewer.bat` : MSBuild を自動検出してコンパイルとコピーをワンクリックで行う開発者用バッチファイル
  * *※共有時には、容量削減のため自動生成される `bin/` フォルダや `obj/` フォルダは削除してから圧縮してください。*

---

## 🛠️ ゲームプロジェクトへの組み込み方法

ご自身のゲームプロジェクト（独自エンジンや DirectX/OpenGL/Unity/Unreal 等のプロジェクト）にこのツールを統合する手順です。

### 1. フォルダの配置
受け取った `PerformanceViewer` フォルダを、ご自身のゲームプロジェクト内の任意の場所（例: `externals/PerformanceViewer/` や `Tools/PerformanceViewer/` など）に配置します。

### 2. ビルド依存関係の設定 (Visual Studio)
C++ソリューションに組み込む場合は、以下の設定を行っておくと便利です。
1. ソリューションを右クリック ＞「追加」＞「既存のプロジェクト」を選択し、配置した `PerformanceViewer.vcxproj` を追加します。
2. ソリューションを右クリック ＞「プロジェクトの依存関係」を開きます。
3. ご自身のゲーム（メインプロジェクト）を選択し、依存先に `PerformanceViewer` にチェックを入れます。
   * これにより、ゲームのビルド時にビューワーのコード変更も自動追従してビルドされます。

### 3. ゲーム側プログラムへの起動・通知コードの実装
ゲーム側からビューワーを起動、あるいはすでに起動しているビューワーに対して新しいレポートのロード通知を行うための C++ 実装コード例です。

> [!IMPORTANT]
> **多重起動（二重起動）の防止について**
> ゲームがスパイクを検知するたびに無条件で `ShellExecuteA` を呼んでしまうと、ビューワーのプロセスが画面裏で何個も重複して立ち上がってしまいます。
> これを防ぐため、以下の実装例では、まずポート `8080` に対してソケットでヘルスチェックを送信し、すでにビューワーが起動しているかを検知しています。
> - **すでに起動している場合**: `update_path` API に最新レポートのパスを投げて既存の画面を更新させます（新プロセスは起動しません）。
> - **起動していない場合**: `ShellExecuteA` を呼び出して新規プロセスとして起動します。

```cpp
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <format>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

// ビューワーサーバーへ連携を行う関数
void TriggerPerformanceViewer(const std::string& reportJsonAbsPath) {
    // 1. WinSock 初期化
    WSADATA wsaData;
    bool wsaInit = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);

    bool alreadyRunning = false;

    if (wsaInit) {
        // すでにビューワーがポート 8080 で起動しているかを確認するソケット
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            // タイムアウト設定 (応答がない場合にフリーズするのを防ぐ)
            DWORD timeout = 150; // 150ms
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
                // ヘルスチェックリクエストを送信して本アプリか確認
                std::string checkReq = "GET /check_viewer HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";
                send(sock, checkReq.c_str(), (int)checkReq.size(), 0);

                char buf[256]{};
                int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
                if (bytes > 0 && std::string(buf).find("PerformanceViewer") != std::string::npos) {
                    alreadyRunning = true;
                }
            }
            closesocket(sock);
        }
    }

    if (alreadyRunning) {
        // 2. すでに起動中の場合は、新しいレポートパスを通知してリロードを促す (二重起動を防ぐ)
        SOCKET notifySock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (notifySock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8080);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            if (connect(notifySock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
                // パス内の '\\' を '/' に統一 (URL仕様の互換性のため)
                std::string cleanPath = reportJsonAbsPath;
                std::replace(cleanPath.begin(), cleanPath.end(), '\\', '/');

                // HTTP GET リクエストを送信してロードを指示
                std::string updateReq = std::format(
                    "GET /update_path?path={} HTTP/1.1\r\n"
                    "Host: localhost:8080\r\n"
                    "Connection: close\r\n\r\n",
                    cleanPath
                );
                send(notifySock, updateReq.c_str(), (int)updateReq.size(), 0);
                
                // サーバーからの受領完了応答を待ってからソケットを閉じる
                char dummy[64];
                recv(notifySock, dummy, sizeof(dummy), 0);
            }
            closesocket(notifySock);
        }
    } else {
        // 3. 起動していない場合は、ShellExecuteA で新しく起動する
        // 【注意】ご自身のプロジェクト内での配置パスに合わせて書き換えてください！
        std::string relExePath = "externals/PerformanceViewer/PerformanceViewer.exe";
        std::string relWorkDir = "externals/PerformanceViewer";

        if (std::filesystem::exists(relExePath)) {
            std::string targetExe = std::filesystem::absolute(relExePath).string();
            std::string workDir = std::filesystem::absolute(relWorkDir).string();
            ShellExecuteA(NULL, "open", targetExe.c_str(), NULL, workDir.c_str(), SW_SHOW);
        }
    }

    if (wsaInit) {
        WSACleanup();
    }
}
```

#### 💡 この関数を呼び出すタイミングの推奨フロー
* **自動起動（高負荷スパイク検知時）**
  ゲームループ内でFPSを毎フレーム監視し、一時的に設定しきい値（例：30 FPS以下）を割った瞬間、あるいはフレーム処理時間が急激に跳ね上がったスパイク検知のタイミングで、計測ファイル（`system_log.json`）を書き出した後に、その絶対パスを引数に渡して `TriggerPerformanceViewer(reportJsonAbsPath)` を呼び出します。
* **手動起動（デバッグオプション時）**
  ゲームの起動時のコマンドライン引数（例: `-dev_perf`）が指定されている場合、またはゲーム内で特定のデバッグキー（例: `F11`）が押された際に、ゲーム開始時や任意のタイミングで `TriggerPerformanceViewer("")` （空文字を渡すことで、新規起動のみ行う）を呼び出してリアルタイム監視を開始します。

---

## 🔌 ゲームとビューワーの通信 API 仕様 (独自エンジン向け)

独自エンジンで計測したパフォーマンス情報を、起動したビューワーへリアルタイム送信したり、スパイク発生時のレポートロードを行うための通信インタフェース（HTTP REST API 仕様）です。

### 1. リアルタイム監視データの定期送信
ゲームの実行中、ゲームエンジン内のスレッドまたは定期タイマー等を用いて、**300ms間隔程度**で以下のHTTP GETリクエストをビューワーに送信します。これにより、ビューワー画面上に現在のFPSやメモリ使用量がリアルタイムで描画されます。

* **URL:** `GET http://localhost:8080/update_live_metrics`
* **パラメータ仕様:**
  * `fps` (float): 現在の瞬時FPS値
  * `cpu` (float): 現在のゲーム全体のCPU物理メモリ使用量 (MB)
  * `vram` (float): 現在のビデオメモリ (GPU VRAM) 使用量 (MB)
  * `time` (float): ゲーム開始からの経過時間 (秒単位、X軸の座標に使用)

**リクエストURL例:**
`http://localhost:8080/update_live_metrics?fps=58.5&cpu=452.3&vram=128.0&time=12.4`

### 2. 高負荷スパイク検出時のレポート連携
ゲーム内でFPS低下などのスパイクを検出した際、データをロードさせるフローです。

> [!WARNING]
> **フォルダー構造とファイル名に関する重要な制約**
> ビューワー側の自動データ探索ロジックには、以下の厳しい階層・命名の制約があります。これに沿わない名前で出力された場合、ビューワーはファイルを検知できず、グラフやログが空で表示されます。
> 
> * **フォルダー名**: 必ず **`report_` から始まる名前** のサブフォルダーにしてください（例: `report_2026-07-21_153020`）。`report_` 以外の名前のフォルダーは、ビューワーの探索対象から無視されます。
> * **ファイル名**: フォルダーの中に出力するJSONファイルの名前は、日付などを入れず、必ず固定で **`system_log.json`** としてください。
> 
> **【正しい配置構造】:**
> `[ビューワーフォルダ]/out/performance_reports/report_YYYYMMDD_HHMMSS/system_log.json`
> `[ビューワーフォルダ]/out/performance_reports/report_YYYYMMDD_HHMMSS/replay.mp4` (動画がある場合)

* **手順 A (ファイルの保存)**:
  ゲーム側で、直前3秒間のパフォーマンス推移データである `system_log.json`（データ仕様は後述）および、キャプチャ動画ファイルを、ビューワーの出力フォルダ `out/performance_reports/report_[任意の個別識別名]/` 配下に保存します。
* **手順 B (ビューワーへの通知)**:
  前述の `TriggerPerformanceViewer(reportJsonAbsPath)` を呼び出すことで、すでにビューワーが起動していれば `/update_path` API が自動的に既存サーバーへ投げられ、起動していなければ自動で新規起動されます。

---

## 🚀 使用方法

### 1. 自動起動
ゲーム側で上記の実装が完了していれば、ゲーム実行中に重くなったタイミングでブラウザが自動的に開き、パフォーマンスレポートが描画されます。

### 2. 手動起動
`PerformanceViewer.exe` を直接ダブルクリックして起動することも可能です。
* 起動すると、自動的にブラウザが開きます。
* ツール内部の `out/performance_reports/` 配下に存在する最新のレポートが自動でロードして表示されます。

### 3. 開発者向け：自動ビルドバッチによるビルド (推奨)
C++ソースファイルを変更してビューワーを再ビルドする場合、`build_viewer.bat` をダブルクリックして実行するだけで簡単にビルドが完了します。
* 実行すると、スクリプトがローカル of `MSBuild.exe` を自動検出します。
* ビルド構成を `Debug` (1) または `Release` (2) から選択すると、自動でコンパイル・コピーされ、ツール本体が更新されます。
* 既に起動中の `PerformanceViewer.exe` が存在していても、一時ファイルに自動で退避させてから上書きコピーするため、ファイルロックによるビルドエラーになりません。

---

## ⚙️ 各種モードの説明

### 1. 🎬 リプレイ解析モード
* 高負荷スパイクが検知された瞬間の「直前3秒間」のゲーム画面キャプチャ動画と、その時のFPS・メモリの推移グラフを同期して再生・確認できます。
* **AI解析アシスタント機能**：画面右にあるプロンプトエリアの「コピー」ボタンを押し、ChatGPTやGemini等のLLMに貼り付けることで、ボトルネックの自動解析依頼が簡単に行えます。

### 2. 🟢 リアルタイム監視モード
* ゲームの動作中に、現在のFPS、CPUメモリ使用量、VRAM使用量をリアルタイムでグラフ上に描画し、常時監視できます。
* **リアルタイム統計ダッシュボード**：画面右半分に、監視セッション中の平均FPS、最小FPS（1% Lowベース）、最大メモリ使用量のサマリー情報が動的に計算されて表示されます。
* **重要警告ログモニター**：ゲーム側から送信された最新ログの中から、`[警告]` や `error` 、`fail` 、`低速ロード` など、開発者が対処すべき重要システムログのみをフィルタリングしてリアルタイムでストリーミング表示します。

---

## 🛑 終了方法および多重起動時の挙動

### 1. 安全な自動シャットダウンとクリーンアップ
* ビューワー画面の右上部にある「**終了する**」ボタンをクリックすると、サーバープロセスが正常終了し、退避用の一時ファイル（`PerformanceViewer.exe.old`）も自動的にきれいに削除されます。
* 「終了する」ボタンを押さずに、**ブラウザのタブやウィンドウを直接「×」で閉じた場合**でも、ブラウザ切断イベント（`beforeunload`）を検知して自動的にサーバープロセスが正常終了し、一時ファイルもクリーンアップされます（ゾンビプロセスの発生を防ぎます）。

### 2. 多重起動（二重起動）時の自動連携
* 既にツールがポート `8080` で起動している状態で、別の構成（例: Release版）を実行しようとした場合、新プロセスは既存プロセスへ現在のビルドモード情報を上書き通知した上で、自動的にブラウザで既存サーバーの画面を開き、自身はサーバーを起動せずに静かに終了します。
* ポート `8080` が無関係の他アプリで占有されている場合は、空きポートが見つかるまで自動でポートを `+1`（8081, 8082...）して起動し、自動でブラウザを開くポートフォールバックを搭載しています。

---

## 🛠️ 高度な設定：system_log.json のデータ仕様

他の人が独自にゲームエンジンを開発して本ツールと連携させる場合、以下のフォーマットで `system_log.json` を書き出すことで、ツール側がデータを自動解析し、高度なAI解析用プロンプトを動的に生成します。

### JSONフォーマット例
```json
{
  "reason": "FPS_DROP", 
  "detail": "Bunnyモデルロード時に FPS が 2.4 に低下しました。",
  "time_triggered": "2026-07-21 12:20:47",
  "resolution": "1280x720",
  "logs": [
    {
      "time": 0.300,
      "fps": 60.00,
      "memory_mb": 430.50,
      "vram_mb": 204.10
    },
    {
      "time": 0.600,
      "fps": 2.40,
      "memory_mb": 666.20,
      "vram_mb": 204.10
    }
  ],
  "system_logs": [
    "[システム] エンジン起動完了",
    "├─ 【テクスチャロード完了】 : resources/Textures/white.png | 所要時間: 0.2029秒",
    "[警告] ★高負荷スパイク検知: FPSが一時的に低下しました (2.4 FPS)"
  ]
}
```

### 💡 AI解析プロンプトの自動生成ロジック
ツール（`app.js`）は `system_log.json` の中身を自動的に解析し、以下の情報を組み込んだ詳細なプロンプトを作成します：
1. **最低FPSの自動特定**：何秒時点で最低何FPSまで低下したかを自動計算して指示に含めます。
2. **メモリ推移の自動計算**：開始時から終了時にかけてCPUメモリやGPU VRAMが何MB増減したかを算出し、メモリリークの診断をLLMに指示します。
3. **ボトルネック候補の自動抽出**：`system_logs` から「低速ロード」や「警告・エラー」などのキーワードを含む行を自動抽出し、アセット処理の問題か、それ以外の描画ロジックの問題かを自動的に判定してプロンプトに追加します。
