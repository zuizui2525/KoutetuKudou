#pragma once

#include "Engine/Math/MathStructs.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

/// <summary>
/// マップエディターおよびマップレンダラーで共有する定数・データ構造体の定義。
/// 「空白」と「ビル」のみに特化し、マジックナンバーを排除した設計です。
/// </summary>

// ============================================================
// マップグリッド基本定数
// ============================================================

/// <summary>デフォルトのマップグリッド幅（X方向のセル数：約104m）</summary>
inline constexpr int kDefaultGridWidth = 26;

/// <summary>デフォルトのマップグリッド奥行き（Z方向のセル数：1エリア長120m / 4m = 30セル）</summary>
inline constexpr int kDefaultGridHeight = 30;

/// <summary>1セルのワールド空間サイズ（メートル単位）</summary>
inline constexpr float kDefaultTileSize = 4.0f;

/// <summary>デフォルトのビルの高さ（メートル単位）</summary>
inline constexpr float kDefaultBuildingHeight = 12.0f;

/// <summary>最小マス目数</summary>
inline constexpr int kMinGridDimension = 1;

/// <summary>最大マス目数</summary>
inline constexpr int kMaxGridDimension = 2000;

// ============================================================
// マップチップ種別ID定数（空白とビルのみ）
// ============================================================

/// <summary>空白（何も置かれていない通路空間）</summary>
inline constexpr int kTileEmpty = 0;

/// <summary>ビル（立体障害物ブロック）</summary>
inline constexpr int kTileBuilding = 1;

// ============================================================
// ビル表示用定数
// ============================================================

/// <summary>ビルのデフォルトカラー（モダンなコンクリート・メタリックブルー）</summary>
inline constexpr Vector4 kDefaultBuildingColor = { 0.70f, 0.78f, 0.88f, 1.0f };

/// <summary>ハイライト枠のオフセット（チラつき防止）</summary>
inline constexpr float kHighlightOffsetFactor = 0.5f;

/// <summary>
/// 単一のビル配置データ
/// </summary>
struct BuildingCoord {
    int x = 0;
    int z = 0;
};

/// <summary>
/// グリッド全体を表すマップデータ構造体（ビル配置特化型）
/// </summary>
struct MapGrid {
    int width = kDefaultGridWidth;
    int height = kDefaultGridHeight;
    float tileSize = kDefaultTileSize;
    float buildingHeight = kDefaultBuildingHeight;
    Vector4 buildingColor = kDefaultBuildingColor;

    // 2DタイルID配列 [z][x] (0: 空白, 1: ビル)
    std::vector<std::vector<int>> tiles;

    MapGrid() : width(kDefaultGridWidth), height(kDefaultGridHeight), tileSize(kDefaultTileSize), buildingHeight(kDefaultBuildingHeight) {
        Reset(kDefaultGridWidth, kDefaultGridHeight);
    }

    /// <summary>
    /// グリッドサイズを指定して初期化・全消去
    /// </summary>
    void Reset(int newWidth, int newHeight) {
        width = (std::max)(kMinGridDimension, (std::min)(kMaxGridDimension, newWidth));
        height = (std::max)(kMinGridDimension, (std::min)(kMaxGridDimension, newHeight));
        tiles.assign(static_cast<size_t>(height), std::vector<int>(static_cast<size_t>(width), kTileEmpty));
    }

    /// <summary>
    /// 指定マスのタイル種別を取得（範囲外は空白）
    /// </summary>
    int GetTile(int x, int z) const {
        if (x < 0 || x >= width || z < 0 || z >= height) {
            return kTileEmpty;
        }
        return tiles[static_cast<size_t>(z)][static_cast<size_t>(x)];
    }

    /// <summary>
    /// 指定マスにタイル種別を設定（0: 空白, 1: ビル）
    /// </summary>
    bool SetTile(int x, int z, int tileId) {
        if (x < 0 || x >= width || z < 0 || z >= height) {
            return false;
        }
        tiles[static_cast<size_t>(z)][static_cast<size_t>(x)] = (tileId == kTileBuilding) ? kTileBuilding : kTileEmpty;
        return true;
    }

    /// <summary>
    /// 現在配置されているビルの総数を取得
    /// </summary>
    size_t GetBuildingCount() const {
        size_t count = 0;
        for (int z = 0; z < height; ++z) {
            for (int x = 0; x < width; ++x) {
                if (tiles[static_cast<size_t>(z)][static_cast<size_t>(x)] == kTileBuilding) {
                    count++;
                }
            }
        }
        return count;
    }

    /// <summary>
    /// 配置されているビルの座標一覧を取得
    /// </summary>
    std::vector<BuildingCoord> GetBuildingCoords() const {
        std::vector<BuildingCoord> coords;
        for (int z = 0; z < height; ++z) {
            for (int x = 0; x < width; ++x) {
                if (tiles[static_cast<size_t>(z)][static_cast<size_t>(x)] == kTileBuilding) {
                    coords.push_back({ x, z });
                }
            }
        }
        return coords;
    }
};
