// MatrixBuffer: 변환 행렬 관리
cbuffer OcclusionObjectInfo : register(b0)
{
    float3 Pos;
    float Scale;
};

cbuffer MatrixConstants : register(b1)
{
    row_major float4x4 ViewProjection;
    float3 cameraPos;
};

Texture2D DepthMap : register(t0);
SamplerState MapSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION; // 변환된 화면 좌표
};

const static float2 PolygonPos[30] =
{
    // 9개의 삼각형을 만드는 버텍스들
    // 각 삼각형의 3개 정점 정의 (중앙 점 (0, 0) + 원 둘레의 점들)

    // 삼각형 1
    float2(0.0000, 0.0000), float2(1.0000, 0.0000), float2(0.7660, 0.6428),
    // 삼각형 2
    float2(0.0000, 0.0000), float2(0.7660, 0.6428), float2(0.3420, 0.9397),
    // 삼각형 3
    float2(0.0000, 0.0000), float2(0.3420, 0.9397), float2(-0.3420, 0.9397),
    // 삼각형 4
    float2(0.0000, 0.0000), float2(-0.3420, 0.9397), float2(-0.7660, 0.6428),
    // 삼각형 5
    float2(0.0000, 0.0000), float2(-0.7660, 0.6428), float2(-1.0000, 0.0000),
    // 삼각형 6
    float2(0.0000, 0.0000), float2(-1.0000, 0.0000), float2(-0.7660, -0.6428),
    // 삼각형 7
    float2(0.0000, 0.0000), float2(-0.7660, -0.6428), float2(-0.3420, -0.9397),
    // 삼각형 8
    float2(0.0000, 0.0000), float2(-0.3420, -0.9397), float2(0.3420, -0.9397),
    // 삼각형 9
    float2(0.0000, 0.0000), float2(0.3420, -0.9397), float2(0.7660, -0.6428),
    // 삼각형 10
    float2(0.0000, 0.0000), float2(0.7660, -0.6428), float2(1.0000, 0.0000)
};

float GetDepth(float2 uv)
{
    return DepthMap.Sample(MapSampler, uv);
}

PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;

    // 카메라를 향하는 방향 벡터
    float3 forward = normalize(cameraPos - Pos);
    
    // 기본적인 업 벡터
    float3 up = float3(0, 1, 0);
    
    // 우측 벡터 (right) 계산
    float3 right = normalize(cross(up, forward));
    
    // 새로운 업 벡터 계산 (정확한 정렬을 위해)
    up = cross(forward, right);

    // 정점 변환을 위한 행렬 생성
    float3 quadVertex = PolygonPos[vertexID].x * right + PolygonPos[vertexID].y * up;

    // 정점 변환
    output.position = float4(quadVertex * Scale + Pos, 1.0);
    output.position = mul(output.position, ViewProjection);
    
    return output;
}
float4 mainPS(PS_INPUT input) : SV_Target
{
    float depth = input.position.z / input.position.w;
    
    // Query 개수새기용.
    if (depth > GetDepth(input.position.xy))
    {
        discard;
    }
    return float4(1.0, 1.0, 1.0, 1.0);
}
