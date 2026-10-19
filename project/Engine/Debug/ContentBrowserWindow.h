#pragma once
#ifdef _USEIMGUI
#include <filesystem>
#include <string>
#include <vector>
#include <d3d12.h>

class ContentBrowserWindow {
public:
    ContentBrowserWindow();
    ~ContentBrowserWindow() = default;

    /// <summary>
    /// Content Browser ウィンドウの描画
    /// </summary>
    /// <param name="pOpen">ウィンドウ表示フラグ</param>
    void Draw(bool* pOpen);

private:
    void EnsureIconsLoaded();
    D3D12_GPU_DESCRIPTOR_HANDLE GetIconHandleForEntry(const std::filesystem::directory_entry& entry);

    // ナビゲーション操作
    void NavigateTo(const std::filesystem::path& newPath);
    void GoBack();
    void GoForward();
    void GoUp();
    bool CanGoBack() const;
    bool CanGoForward() const;
    bool CanGoUp() const;

    // UI描画
    void DrawNavigationBar();
    void DrawBreadcrumbs();
    void DrawFileGrid();
    void DrawItemNode(const std::filesystem::directory_entry& entry, float thumbnailSize, float padding);
    void DrawItemTooltip(const std::filesystem::directory_entry& entry);

    // ファイル情報フォーマット補助
    std::string GetFileTypeDescription(const std::filesystem::directory_entry& entry) const;
    std::string FormatFileSize(uintmax_t bytes) const;
    std::string FormatFileTime(std::filesystem::file_time_type ftime) const;

    // ファイル操作
    void DuplicateEntry(const std::filesystem::path& targetPath);
    void DeleteEntry(const std::filesystem::path& targetPath);
    void ShowInExplorer(const std::filesystem::path& targetPath) const;
    void DrawDeleteConfirmModal();

    enum class RootCategory {
        Resources,
        Scripts
    };

    void SwitchRootCategory(RootCategory category);

    RootCategory currentCategory_ = RootCategory::Resources;
    std::filesystem::path resourcesRoot_ = "resources";
    std::filesystem::path scriptsRoot_ = "Scripts";
    std::filesystem::path rootDirectory_;
    std::filesystem::path currentDirectory_;
    std::vector<std::filesystem::path> history_;
    size_t historyIndex_ = 0;
    bool iconsLoaded_ = false;

    // 削除確認モーダル用ステート変数
    std::filesystem::path itemToDelete_;
    bool showDeleteConfirmModal_ = false;

    // マジックナンバー排除のためのUIレイアウト定数
    static constexpr float kDefaultThumbnailSize = 64.0f;
    static constexpr float kThumbnailPadding = 16.0f;
    static constexpr float kNavButtonHeight = 24.0f;
    static constexpr float kNavArrowButtonWidth = 28.0f;
    static constexpr float kUpButtonWidth = 46.0f;
    static constexpr float kItemSpacing = 8.0f;
    static constexpr size_t kBaseFilenameLength = 12; // 基本表示文字数（サムネイルサイズに応じて拡張）
    static constexpr float kTooltipImagePreviewSize = 128.0f; // ツールチップ内画像プレビューの表示サイズ

    float thumbnailSize_ = kDefaultThumbnailSize;
};
#endif
