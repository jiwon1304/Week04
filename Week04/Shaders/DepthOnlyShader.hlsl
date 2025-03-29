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


// campos, viewproj, location, scale. 
// rotation은 자체적으로 계산

// 매 드로우콜마다 보내야 하는거 -> campos, scale
// 매 프레임마다 보내야 하는거 -> viewproj

struct PS_INPUT
{
    float4 position : SV_POSITION; // 변환된 화면 좌표
};

//// 정점 인덱스에 따라 Quad 정점 좌표 생성
//const float2 QuadPos[6] =
//{
//    float2(-1, -1), float2(1, -1), float2(-1, 1), // 첫 번째 삼각형
//    float2(-1, 1), float2(1, -1), float2(1, 1) // 두 번째 삼각형
//};

PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    float2 QuadPos[6] =
    {
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        float2(-1, 1), float2(1, -1), float2(1, 1)
    };
    
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
    float3 quadVertex = QuadPos[vertexID].x * right + QuadPos[vertexID].y * up;

    // 정점 변환
    output.position = float4(quadVertex * Scale + Pos, 1.0);
    output.position = mul(output.position, ViewProjection);
    
    return output;
}
float4 mainPS(PS_INPUT input) : SV_Target
{
    float depth = input.position.z / input.position.w;
    
    return float4(depth, depth, depth, 1.0);
}
