#include "ShaderBuffers.hlsl"

// UAV 선언
RWTexture2D<uint> UUIDTextureWrite : register(u0);

struct VS_INPUT
{
    float3 position : POSITION; // 버텍스 위치
};

struct PS_INPUT
{
    float4 position : SV_Position;
};


PS_INPUT MainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);
    output.position = mul(output.position, WorldMatrix);
    output.position = mul(output.position, ViewMatrix);
    output.position = mul(output.position, ProjectionMatrix);
    
    return output;
}

// output color 없음. depth만 기록하고, UUID는 UUIDBuffer에 기록(이후 compute shader에서 사용)
void MainPS(PS_INPUT input)
{
    int2 coord = int2(input.position.xy);
    UUIDTextureWrite[coord] = UUID; // UAV에 UUID 저장
}

// ComputeShader
// UUID가 저장된 UAV를 읽고 리스트를 생성
Texture2D<uint> UUIDTextureRead : register(t0);
RWStructuredBuffer<uint> UUIDList : register(u1);

[numthreads(16, 16, 1)]
void MainCS(uint3 DTid : SV_DispatchThreadID)
{
    int2 pixelPos = DTid.xy;
    uint uuid = UUIDTextureRead.Load(int3(pixelPos, 0));

    if (uuid != 0)
    { // 배경이 아니면 리스트에 저장
        uint index;
        InterlockedAdd(UUIDList[0], 1, index);
        UUIDList[index + 1] = uuid;
    }
}