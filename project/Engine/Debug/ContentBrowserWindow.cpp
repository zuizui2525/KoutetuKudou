#ifdef _USEIMGUI
#include "Engine/Debug/ContentBrowserWindow.h"
#include "externals/imgui/imgui.h"
#include "Engine/Base/BaseResource.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace {
    // ペイロード識別子定数（ドラッグ＆ドロップ用）
    constexpr const char* kPayloadTexture = "CONTENT_BROWSER_ITEM_TEXTURE";
    constexpr const char* kPayloadModel   = "CONTENT_BROWSER_ITEM_MODEL";
    constexpr const char* kPayloadAudio   = "CONTENT_BROWSER_ITEM_AUDIO";
    constexpr const char* kPayloadScene   = "CONTENT_BROWSER_ITEM_SCENE";
    constexpr const char* kPayloadScript  = "CONTENT_BROWSER_SCRIPT";
    constexpr const char* kPayloadPrefab  = "CONTENT_BROWSER_PREFAB";
    constexpr const char* kPayloadGeneric = "CONTENT_BROWSER_ITEM_GENERIC";

    // 拡張子判定用
    bool IsTextureExtension(const std::string& ext) {
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga";
    }

    bool IsModelExtension(const std::string& ext) {
        return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx";
    }

    bool IsAudioExtension(const std::string& ext) {
        return ext == ".wav" || ext == ".mp3" || ext == ".ogg";
    }

    bool IsSceneExtension(const std::string& ext) {
        return ext == ".json";
    }

    bool IsScriptExtension(const std::string& ext) {
        return ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".c";
    }

    bool IsPrefabExtension(const std::string& ext) {
        return ext == ".prefab";
    }

    const char* GetPayloadType(const std::string& ext) {
        if (IsTextureExtension(ext)) return kPayloadTexture;
        if (IsModelExtension(ext)) return kPayloadModel;
        if (IsAudioExtension(ext)) return kPayloadAudio;
        if (IsSceneExtension(ext)) return kPayloadScene;
        if (IsScriptExtension(ext)) return kPayloadScript;
        if (IsPrefabExtension(ext)) return kPayloadPrefab;
        return kPayloadGeneric;
    }

    // アイコンキー名定数
    constexpr const char* kIconFolderKey  = "sys_icon_folder";
    constexpr const char* kIconTextureKey = "sys_icon_texture";
    constexpr const char* kIconModelKey   = "sys_icon_model";
    constexpr const char* kIconAudioKey   = "sys_icon_audio";
    constexpr const char* kIconSceneKey   = "sys_icon_scene";
    constexpr const char* kIconFileKey    = "sys_icon_file";

    // アイコンファイルパス定数
    constexpr const char* kIconFolderPath  = "resources/Textures/Icons/icon_folder.png";
    constexpr const char* kIconTexturePath = "resources/Textures/Icons/icon_texture.png";
    constexpr const char* kIconModelPath   = "resources/Textures/Icons/icon_model.png";
    constexpr const char* kIconAudioPath   = "resources/Textures/Icons/icon_audio.png";
    constexpr const char* kIconScenePath   = "resources/Textures/Icons/icon_scene.png";
    constexpr const char* kIconFilePath    = "resources/Textures/Icons/icon_file.png";
}

ContentBrowserWindow::ContentBrowserWindow()
    : currentCategory_(RootCategory::Resources),
      resourcesRoot_("resources"),
      scriptsRoot_("Scripts"),
      rootDirectory_(resourcesRoot_),
      currentDirectory_(rootDirectory_),
      history_({ rootDirectory_ }),
      historyIndex_(0) {
    std::filesystem::create_directories(resourcesRoot_);
    std::filesystem::create_directories(scriptsRoot_);
}

void ContentBrowserWindow::SwitchRootCategory(RootCategory category) {
    if (currentCategory_ == category) return;

    currentCategory_ = category;
    rootDirectory_ = (currentCategory_ == RootCategory::Resources) ? resourcesRoot_ : scriptsRoot_;
    std::filesystem::create_directories(rootDirectory_);

    history_.clear();
    history_.push_back(rootDirectory_);
    historyIndex_ = 0;
    currentDirectory_ = rootDirectory_;
}

void ContentBrowserWindow::NavigateTo(const std::filesystem::path& newPath) {
    if (newPath == currentDirectory_) return;

    // 現在地より先の「進む」履歴を削除
    if (historyIndex_ + 1 < history_.size()) {
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(historyIndex_ + 1), history_.end());
    }

    history_.push_back(newPath);
    historyIndex_ = history_.size() - 1;
    currentDirectory_ = newPath;
}

void ContentBrowserWindow::GoBack() {
    if (CanGoBack()) {
        historyIndex_--;
        currentDirectory_ = history_[historyIndex_];
    }
}

void ContentBrowserWindow::GoForward() {
    if (CanGoForward()) {
        historyIndex_++;
        currentDirectory_ = history_[historyIndex_];
    }
}

void ContentBrowserWindow::GoUp() {
    if (CanGoUp()) {
        NavigateTo(currentDirectory_.parent_path());
    }
}

bool ContentBrowserWindow::CanGoBack() const {
    return historyIndex_ > 0;
}

bool ContentBrowserWindow::CanGoForward() const {
    return historyIndex_ + 1 < history_.size();
}

bool ContentBrowserWindow::CanGoUp() const {
    return currentDirectory_ != rootDirectory_ && currentDirectory_.has_parent_path();
}

void ContentBrowserWindow::EnsureIconsLoaded() {
    if (iconsLoaded_) return;

    if (auto texMgr = TextureResource::GetTextureManager()) {
        texMgr->LoadTexture(kIconFolderKey, kIconFolderPath);
        texMgr->LoadTexture(kIconTextureKey, kIconTexturePath);
        texMgr->LoadTexture(kIconModelKey, kIconModelPath);
        texMgr->LoadTexture(kIconAudioKey, kIconAudioPath);
        texMgr->LoadTexture(kIconSceneKey, kIconScenePath);
        texMgr->LoadTexture(kIconFileKey, kIconFilePath);
        iconsLoaded_ = true;
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE ContentBrowserWindow::GetIconHandleForEntry(const std::filesystem::directory_entry& entry) {
    auto texMgr = TextureResource::GetTextureManager();
    if (!texMgr) {
        D3D12_GPU_DESCRIPTOR_HANDLE nullHandle{};
        nullHandle.ptr = 0;
        return nullHandle;
    }

    if (entry.is_directory()) {
        return texMgr->GetGpuHandle(kIconFolderKey);
    }

    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (IsTextureExtension(ext)) {
        std::string pathKey = entry.path().generic_string();
        // 未ロードの画像であればオンデマンドでロード
        if (!texMgr->HasTexture(pathKey)) {
            texMgr->LoadTextureByPath(pathKey);
        }
        if (texMgr->HasTexture(pathKey)) {
            return texMgr->GetGpuHandle(pathKey);
        }
        return texMgr->GetGpuHandle(kIconTextureKey);
    }
    if (IsModelExtension(ext)) {
        return texMgr->GetGpuHandle(kIconModelKey);
    }
    if (IsAudioExtension(ext)) {
        return texMgr->GetGpuHandle(kIconAudioKey);
    }
    if (IsSceneExtension(ext)) {
        return texMgr->GetGpuHandle(kIconSceneKey);
    }
    if (IsPrefabExtension(ext)) {
        return texMgr->GetGpuHandle(kIconModelKey);
    }

    return texMgr->GetGpuHandle(kIconFileKey);
}

void ContentBrowserWindow::Draw(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    EnsureIconsLoaded();

    if (ImGui::Begin("コンテンツブラウザ###ContentBrowser", pOpen)) {
        DrawNavigationBar();
        ImGui::Separator();
        DrawFileGrid();

        // 空白エリアの右クリックメニュー
        if (ImGui::BeginPopupContextWindow("ContentBrowserBgContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("エクスプローラーでこのフォルダを開く")) {
                std::string fullPath = std::filesystem::absolute(currentDirectory_).string();
                ShellExecuteA(nullptr, "open", fullPath.c_str(), nullptr, nullptr, SW_SHOW);
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();

    DrawDeleteConfirmModal();
}

void ContentBrowserWindow::DrawNavigationBar() {
    // 0. ルートカテゴリ切替ボタングループ ([Resources] / [Scripts])
    bool isRes = (currentCategory_ == RootCategory::Resources);
    bool isScr = (currentCategory_ == RootCategory::Scripts);

    constexpr ImVec4 kActiveCategoryColor = ImVec4(0.2f, 0.55f, 0.85f, 1.0f);
    if (isRes) ImGui::PushStyleColor(ImGuiCol_Button, kActiveCategoryColor);
    if (ImGui::Button("Resources##CategoryRes", ImVec2(0.0f, kNavButtonHeight))) {
        SwitchRootCategory(RootCategory::Resources);
    }
    if (isRes) ImGui::PopStyleColor();

    ImGui::SameLine();

    if (isScr) ImGui::PushStyleColor(ImGuiCol_Button, kActiveCategoryColor);
    if (ImGui::Button("Scripts##CategoryScr", ImVec2(0.0f, kNavButtonHeight))) {
        SwitchRootCategory(RootCategory::Scripts);
    }
    if (isScr) ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // 1. [ < ] 戻る (Back) ボタン
    bool canGoBack = CanGoBack();
    if (!canGoBack) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("<##NavBack", ImVec2(kNavArrowButtonWidth, kNavButtonHeight))) {
        GoBack();
    }
    if (!canGoBack) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("戻る");
    }

    ImGui::SameLine();

    // 2. [ > ] 進む (Forward) ボタン
    bool canGoForward = CanGoForward();
    if (!canGoForward) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(">##NavForward", ImVec2(kNavArrowButtonWidth, kNavButtonHeight))) {
        GoForward();
    }
    if (!canGoForward) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("進む");
    }

    ImGui::SameLine();

    // 3. [ ^ ] 親階層へ移動 (Up) ボタン
    bool canGoUp = CanGoUp();
    if (!canGoUp) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("^##NavUp", ImVec2(kNavArrowButtonWidth, kNavButtonHeight))) {
        GoUp();
    }
    if (!canGoUp) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("上の階層へ移動");
    }

    ImGui::SameLine();

    // 4. パンくずリスト表示
    DrawBreadcrumbs();

    // 5. アイコンサイズ調整スライダー（右寄せ気味に配置）
    constexpr float kSliderWidth = 100.0f;
    constexpr float kMinThumbSize = 40.0f;
    constexpr float kMaxThumbSize = 128.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    if (availWidth > kSliderWidth + 10.0f) {
        ImGui::SameLine(ImGui::GetWindowWidth() - kSliderWidth - 25.0f);
        ImGui::SetNextItemWidth(kSliderWidth);
        ImGui::SliderFloat("##IconSize", &thumbnailSize_, kMinThumbSize, kMaxThumbSize, "Size: %.0f");
    }
}

void ContentBrowserWindow::DrawBreadcrumbs() {
    // ルートからの相対パスを分解してボタンにする
    std::filesystem::path rel = std::filesystem::relative(currentDirectory_, rootDirectory_);
    
    // ルートフォルダのボタン
    std::string rootLabel = (currentCategory_ == RootCategory::Resources) 
        ? "Resources##BreadcrumbRoot" 
        : "Scripts##BreadcrumbRoot";
    if (ImGui::Button(rootLabel.c_str(), ImVec2(0.0f, kNavButtonHeight))) {
        NavigateTo(rootDirectory_);
    }

    if (rel != ".") {
        std::filesystem::path accumulated = rootDirectory_;
        for (const auto& part : rel) {
            accumulated /= part;
            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();

            std::string partStr = part.string();
            std::string btnId = partStr + "##Breadcrumb_" + accumulated.string();
            if (ImGui::Button(btnId.c_str(), ImVec2(0.0f, kNavButtonHeight))) {
                NavigateTo(accumulated);
                break;
            }
        }
    }
}

void ContentBrowserWindow::DrawFileGrid() {
    if (!std::filesystem::exists(currentDirectory_)) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "ディレクトリが存在しません: %s", currentDirectory_.string().c_str());
        return;
    }

    float cellSize = thumbnailSize_ + kThumbnailPadding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = static_cast<int>(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    if (ImGui::BeginTable("ContentBrowserTable", columnCount)) {
        // ディレクトリを先に、ファイルを後にソートして一覧化
        std::vector<std::filesystem::directory_entry> directories;
        std::vector<std::filesystem::directory_entry> files;

        for (const auto& entry : std::filesystem::directory_iterator(currentDirectory_)) {
            if (entry.is_directory()) {
                directories.push_back(entry);
            } else {
                files.push_back(entry);
            }
        }

        auto sortByName = [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(directories.begin(), directories.end(), sortByName);
        std::sort(files.begin(), files.end(), sortByName);

        // フォルダ描画
        for (const auto& entry : directories) {
            ImGui::TableNextColumn();
            DrawItemNode(entry, thumbnailSize_, kThumbnailPadding);
        }

        // ファイル描画
        for (const auto& entry : files) {
            ImGui::TableNextColumn();
            DrawItemNode(entry, thumbnailSize_, kThumbnailPadding);
        }

        ImGui::EndTable();
    }
}

void ContentBrowserWindow::DrawItemNode(const std::filesystem::directory_entry& entry, float thumbnailSize, float padding) {
    (void)padding;
    const auto& path = entry.path();
    std::string filename = path.filename().string();
    bool isDir = entry.is_directory();
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    ImGui::PushID(filename.c_str());

    D3D12_GPU_DESCRIPTOR_HANDLE iconHandle = GetIconHandleForEntry(entry);
    ImTextureID texID = (ImTextureID)iconHandle.ptr;

    // タイル全体をグループ化
    ImGui::BeginGroup();

    // ImageButton の描画（マジックナンバー排除）
    constexpr ImVec4 kBgColor = { 0.15f, 0.15f, 0.18f, 0.6f };
    constexpr ImVec4 kHoverCol = { 0.28f, 0.28f, 0.35f, 0.8f };
    constexpr ImVec4 kActiveCol = { 0.38f, 0.38f, 0.48f, 1.0f };
    constexpr ImVec4 kTintColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    ImGui::PushStyleColor(ImGuiCol_Button, kBgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHoverCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kActiveCol);

    std::string btnId = "##btn_" + filename;
    ImGui::ImageButton(btnId.c_str(), texID, ImVec2(thumbnailSize, thumbnailSize), ImVec2(0, 0), ImVec2(1, 1), kBgColor, kTintColor);

    ImGui::PopStyleColor(3);

    // ドラッグ＆ドロップ送信元 (Drag Drop Source)
    if (!isDir) {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            const char* payloadType = GetPayloadType(ext);
            std::string relativePath = path.generic_string();

            // ペイロードとしてファイルパス文字列を送信（末尾のヌル文字を含める）
            ImGui::SetDragDropPayload(payloadType, relativePath.c_str(), (relativePath.length() + 1) * sizeof(char));

            // ドラッグ中のプレビュー描画（アイコン＋ファイル名）
            constexpr float kPreviewIconSize = 24.0f;
            ImGui::Image(texID, ImVec2(kPreviewIconSize, kPreviewIconSize));
            ImGui::SameLine();
            ImGui::TextUnformatted(filename.c_str());
            ImGui::EndDragDropSource();
        }
    }

    // フォルダのダブルクリックで潜る
    if (isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        NavigateTo(path);
    }
    // ファイルのダブルクリックで既定のエディタ（Visual Studio / VS Code 等）で開く
    else if (!isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        std::string fullPath = std::filesystem::absolute(path).string();
        ShellExecuteA(nullptr, "open", fullPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    // サムネイル幅に応じた最大表示文字数の計算（サムネイルサイズ拡大時に長く表示）
    size_t maxLen = kBaseFilenameLength;
    if (thumbnailSize > kDefaultThumbnailSize) {
        float ratio = thumbnailSize / kDefaultThumbnailSize;
        maxLen = static_cast<size_t>(static_cast<float>(kBaseFilenameLength) * ratio);
    }

    // ファイル名（はみ出し防止で一定長で省略）
    std::string displayName = filename;
    constexpr size_t kEllipsisLength = 3;
    if (displayName.length() > maxLen && maxLen > kEllipsisLength) {
        displayName = displayName.substr(0, maxLen - kEllipsisLength) + "...";
    }

    // テキストの中央揃え
    float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
    if (textWidth < thumbnailSize) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (thumbnailSize - textWidth) * 0.5f);
    }
    ImGui::TextUnformatted(displayName.c_str());

    ImGui::EndGroup();

    // アイテムの右クリックコンテキストメニュー (複製・削除・エクスプローラー表示)
    if (ImGui::BeginPopupContextItem("ItemContextMenu")) {
        ImGui::TextDisabled("%s", filename.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("複製 (Duplicate)")) {
            DuplicateEntry(path);
        }

        if (ImGui::MenuItem("削除 (Delete)")) {
            itemToDelete_ = path;
            showDeleteConfirmModal_ = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("エクスプローラーで表示")) {
            ShowInExplorer(path);
        }

        ImGui::EndPopup();
    }

    // アイテム全体（アイコンまたはラベル）にホバーした際にツールチップを表示
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        DrawItemTooltip(entry);
    }

    ImGui::PopID();
}

void ContentBrowserWindow::DrawItemTooltip(const std::filesystem::directory_entry& entry) {
    if (!ImGui::BeginTooltip()) return;

    const auto& path = entry.path();
    std::string filename = path.filename().string();
    bool isDir = entry.is_directory();

    // 1. 完全なファイル名（見出し表示）
    ImGui::TextUnformatted(filename.c_str());
    ImGui::Separator();

    // 画像ファイルの場合は拡大プレビューを表示
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!isDir && IsTextureExtension(ext)) {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = GetIconHandleForEntry(entry);
        if (handle.ptr != 0) {
            ImTextureID previewId = (ImTextureID)handle.ptr;
            ImGui::Image(previewId, ImVec2(kTooltipImagePreviewSize, kTooltipImagePreviewSize));
            ImGui::Separator();
        }
    }

    // 2. 種類
    std::string typeDesc = GetFileTypeDescription(entry);
    ImGui::Text("種類: %s", typeDesc.c_str());

    // 3. サイズ（ファイル）または項目数（フォルダ）
    if (isDir) {
        std::error_code ec;
        size_t count = 0;
        for (auto it = std::filesystem::directory_iterator(path, ec); it != std::filesystem::directory_iterator(); ++it) {
            count++;
        }
        if (!ec) {
            ImGui::Text("項目数: %zu 個", count);
        }
    } else {
        std::error_code ec;
        auto size = entry.file_size(ec);
        if (!ec) {
            ImGui::Text("サイズ: %s", FormatFileSize(size).c_str());
        }
    }

    // 4. 更新日時
    std::error_code ec;
    auto lwt = entry.last_write_time(ec);
    if (!ec) {
        ImGui::Text("更新日時: %s", FormatFileTime(lwt).c_str());
    }

    // 5. 相対パス
    std::string relPath = std::filesystem::relative(path, rootDirectory_).generic_string();
    ImGui::TextDisabled("パス: %s", relPath.c_str());

    ImGui::EndTooltip();
}

std::string ContentBrowserWindow::GetFileTypeDescription(const std::filesystem::directory_entry& entry) const {
    if (entry.is_directory()) {
        return "フォルダー";
    }

    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (IsTextureExtension(ext)) {
        return std::format("テクスチャ画像 ({})", ext);
    }
    if (IsModelExtension(ext)) {
        return std::format("3Dモデル ({})", ext);
    }
    if (IsAudioExtension(ext)) {
        return std::format("オーディオ ({})", ext);
    }
    if (IsSceneExtension(ext)) {
        return std::format("シーンデータ ({})", ext);
    }
    if (IsPrefabExtension(ext)) {
        return std::format("プレハブアセット ({})", ext);
    }
    if (ext == ".ttf" || ext == ".otf") {
        return std::format("フォントファイル ({})", ext);
    }
    if (ext == ".hlsl" || ext == ".hlsli") {
        return std::format("シェーダーファイル ({})", ext);
    }
    if (ext == ".cpp" || ext == ".c") {
        return std::format("C++ ソースファイル ({})", ext);
    }
    if (ext == ".h" || ext == ".hpp") {
        return std::format("C++ ヘッダーファイル ({})", ext);
    }
    if (ext == ".txt" || ext == ".log") {
        return std::format("テキストドキュメント ({})", ext);
    }

    if (ext.empty()) {
        return "ファイル";
    }
    return std::format("{} ファイル", ext);
}

std::string ContentBrowserWindow::FormatFileSize(uintmax_t bytes) const {
    constexpr uintmax_t kBytesInKB = 1024;
    constexpr uintmax_t kBytesInMB = kBytesInKB * 1024;
    constexpr uintmax_t kBytesInGB = kBytesInMB * 1024;

    if (bytes >= kBytesInGB) {
        return std::format("{:.2f} GB ({} バイト)", static_cast<double>(bytes) / static_cast<double>(kBytesInGB), bytes);
    }
    if (bytes >= kBytesInMB) {
        return std::format("{:.2f} MB ({} バイト)", static_cast<double>(bytes) / static_cast<double>(kBytesInMB), bytes);
    }
    if (bytes >= kBytesInKB) {
        return std::format("{:.1f} KB ({} バイト)", static_cast<double>(bytes) / static_cast<double>(kBytesInKB), bytes);
    }
    return std::format("{} バイト", bytes);
}

std::string ContentBrowserWindow::FormatFileTime(std::filesystem::file_time_type ftime) const {
    try {
        // file_time_type を system_clock::time_point に変換
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm{};
        localtime_s(&tm, &cftime);

        constexpr int kYearOffset = 1900;
        constexpr int kMonthOffset = 1;
        return std::format("{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}",
            tm.tm_year + kYearOffset, tm.tm_mon + kMonthOffset, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    } catch (...) {
        return "-";
    }
}

void ContentBrowserWindow::DuplicateEntry(const std::filesystem::path& targetPath) {
    if (!std::filesystem::exists(targetPath)) return;

    std::filesystem::path parent = targetPath.parent_path();
    std::string stem = targetPath.stem().string();
    std::string ext = targetPath.extension().string();

    std::filesystem::path newPath = parent / (stem + "_Copy" + ext);
    int copyIndex = 2;
    while (std::filesystem::exists(newPath)) {
        newPath = parent / (stem + "_Copy" + std::to_string(copyIndex) + ext);
        ++copyIndex;
    }

    try {
        if (std::filesystem::is_directory(targetPath)) {
            std::filesystem::copy(targetPath, newPath, std::filesystem::copy_options::recursive);
        } else {
            std::filesystem::copy_file(targetPath, newPath);
        }
    } catch (const std::exception& e) {
        (void)e;
    }
}

void ContentBrowserWindow::DeleteEntry(const std::filesystem::path& targetPath) {
    if (!std::filesystem::exists(targetPath)) return;

    try {
        if (std::filesystem::is_directory(targetPath)) {
            std::filesystem::remove_all(targetPath);
        } else {
            std::filesystem::remove(targetPath);
        }
    } catch (const std::exception& e) {
        (void)e;
    }
}

void ContentBrowserWindow::ShowInExplorer(const std::filesystem::path& targetPath) const {
    if (!std::filesystem::exists(targetPath)) return;

    std::string fullPath = std::filesystem::absolute(targetPath).string();
    std::string params = "/select,\"" + fullPath + "\"";
    ShellExecuteA(nullptr, "open", "explorer.exe", params.c_str(), nullptr, SW_SHOW);
}

void ContentBrowserWindow::DrawDeleteConfirmModal() {
    if (showDeleteConfirmModal_) {
        ImGui::OpenPopup("アイテムの削除確認###DeleteModal");
    }

    constexpr float kModalBtnWidth = 100.0f;
    constexpr ImVec4 kDangerColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

    if (ImGui::BeginPopupModal("アイテムの削除確認###DeleteModal", &showDeleteConfirmModal_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("本当に以下のアイテムを削除しますか？");
        ImGui::Spacing();
        ImGui::TextColored(kDangerColor, "  %s", itemToDelete_.filename().string().c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("※この操作は取り消せません。ファイルが完全に削除されます。");
        ImGui::Separator();

        if (ImGui::Button("削除", ImVec2(kModalBtnWidth, 0.0f))) {
            DeleteEntry(itemToDelete_);
            showDeleteConfirmModal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("キャンセル", ImVec2(kModalBtnWidth, 0.0f))) {
            showDeleteConfirmModal_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
#endif
