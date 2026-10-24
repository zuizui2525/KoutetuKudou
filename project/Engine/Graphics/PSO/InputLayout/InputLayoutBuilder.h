#pragma once
#include <d3d12.h>
#include <vector>
#include <string>

class InputLayoutBuilder {
public:
    InputLayoutBuilder();
    ~InputLayoutBuilder() = default;

    // 要素を追加 (slot 指定可能、デフォルトは 0)
    void Add(const char* semanticName, DXGI_FORMAT format, uint32_t slot = 0);

    // 最終的に LayoutDesc を作成して返す
    const D3D12_INPUT_LAYOUT_DESC& Build();

private:
    std::vector<std::string> semanticNameStorage_;
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements_;
    uint32_t offset_; // 自動オフセット
    uint32_t currentSlot_ = 0; // 現在のスロット番号

    D3D12_INPUT_LAYOUT_DESC layoutDesc_;
};
