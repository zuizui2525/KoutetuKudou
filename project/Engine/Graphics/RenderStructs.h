#pragma once
#include "Engine/Math/MathStructs.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <map>

// --- 定数バッファ用構造体 (16バイトアライメントに注意) ---

struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvtransform;
    float shininess;
    float environmentCoefficient;
    float padding2[2];
};

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 world;
    Matrix4x4 WorldInverseTranspose;
};

struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 world;
    Vector4 color;
};

struct CameraForGPU {
    Vector3 worldPosition;
    float padding; // 16バイト境界合わせ
};

// --- モデルデータ・頂点定義 ---

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct MaterialData {
    std::string textureFilePath;
};

struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix;
    std::string name;
    std::vector<Node> children;
};

// スキニング用の頂点ウェイト情報 (ModelLoader抽出用)
struct VertexWeightData {
    float weight = 0.0f;
    uint32_t vertexIndex = 0;
};

// ボーンごとの逆バインドポーズ行列と頂点インフルエンスリスト
struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix;
    std::vector<VertexWeightData> vertexWeights;
};

struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    MaterialData material;
    Node rootNode;
    std::map<std::string, JointWeightData> skinClusterData; // ボーン名 -> ウェイト情報
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_INDEX_BUFFER_VIEW ibv{};
};
