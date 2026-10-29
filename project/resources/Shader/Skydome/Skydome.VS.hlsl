#include "Skydome.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // WVPで変換し、Z成分をW成分と同じにすることで、
    // 遠近割り算 (Z/W) での深度値が必ず 1.0 (最奥) になるようにする。
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = input.texcoord;
    output.color = gTransformationMatrix.color;
    return output;
}
