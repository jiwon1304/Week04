#include "ShaderBuffers.hlsl"

// UAV 선언
RWTexture2D<uint> UUIDTextureWrite : register(u1);

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
void mainPS(PS_INPUT input)
{
    int2 coord = int2(input.position.xy);
    //UUIDTextureWrite[float2(0, 0)] = uint(123); // UAV에 UUID 저장
    //UUIDTextureWrite[int(coord.x), int(coord.y)] = uint(1);
    UUIDTextureWrite[int2(input.position.x, input.position.y)] = UUIDuint;
}

////////////////////////////////////////////////
// ComputeShader
// UUID가 저장된 UAV를 읽고 리스트를 생성
Texture2D<uint> UUIDTextureRead : register(t0);
RWStructuredBuffer<uint> UUIDList : register(u2);

[numthreads(16, 16, 1)]
void mainCS(uint3 DTid : SV_DispatchThreadID)
{
    return;
    int2 pixelPos = DTid.xy;
    uint uuid = UUIDTextureRead.Load(int3(pixelPos, 0));
    UUIDList[0] = uuid;

    if (uuid == 0)
        return; // 배경이면 무시

    // 1️⃣ UUID가 이미 있는지 검사
    for (uint i = 1; i <= UUIDList[0]; i++)
    {
        if (UUIDList[i] == uuid)
        {
            return; // 이미 존재하면 추가하지 않음
        }
    }

    uint index;
    InterlockedAdd(UUIDList[0], 1, index);
    UUIDList[index + 1] = uuid;
}
