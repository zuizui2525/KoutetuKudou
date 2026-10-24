#include "Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<Well> gMatrixPalette : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

struct Skinned
{
    float32_t4 position;
    float32_t3 normal;
};

static const int32_t kNumMaxInfluenceInShader = 4;

Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    skinned.position = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    skinned.normal = float32_t3(0.0f, 0.0f, 0.0f);

    // 最大4本のボーンの影響を行列パレットから取得してブレンド
    for (int32_t i = 0; i < kNumMaxInfluenceInShader; ++i)
    {
        skinned.position += input.weight[i] * mul(input.position, gMatrixPalette[input.index[i]].skeletonSpaceMatrix);
        skinned.normal += input.weight[i] * mul(input.normal, (float32_t3x3)gMatrixPalette[input.index[i]].skeletonSpaceInverseTransposeMatrix);
    }
    skinned.position.w = 1.0f;
    skinned.normal = normalize(skinned.normal);
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    Skinned skinned = Skinning(input);

    output.position = mul(skinned.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinned.normal, (float32_t3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinned.position, gTransformationMatrix.World).xyz;
    return output;
}
