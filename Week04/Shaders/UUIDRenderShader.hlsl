#include "ShaderBuffers.hlsl"

struct VS_INPUT
{
    float3 position : POSITION; // 버텍스 위치
};

struct PS_INPUT
{
    float4 position : SV_Position;
};


PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(output.position, WorldMatrix);
    output.position = mul(output.position, ViewMatrix);
    output.position = mul(output.position, ProjectionMatrix);
    
    return output;
}

// output color 없음. depth만 기록하고, UUID는 UUIDBuffer에 기록(이후 compute shader에서 사용)
// 현재 Depth랑 비교 못함....
uint mainPS(PS_INPUT input) : SV_Target
{
    return UUIDuint;
}
