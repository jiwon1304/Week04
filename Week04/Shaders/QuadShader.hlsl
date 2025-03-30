#include "ShaderBuffers.hlsl"

struct VS_INPUT
{
    float3 position : POSITION; // 버텍스 위치
    float2 texcoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // 변환된 화면 좌표
    float2 texcoord : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.f);
    output.texcoord = input.texcoord;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 SampledColor = PrevRenderTexture.Sample(Sampler, input.texcoord);
    return float4(SampledColor, 1.0f);
}
