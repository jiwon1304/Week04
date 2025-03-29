// MatrixBuffer: 변환 행렬 관리
cbuffer MatrixConstants : register(b0)
{
    row_major matrix WorldMatrix;
    float4 UUID;
    bool isSelected;
    float3 MatrixPad0;
};

cbuffer ViewMatrix : register(b5)
{
    row_major matrix ViewMatrix;
}

cbuffer ViewMatrix : register(b6)
{
    row_major matrix ProjectionMatrix;
}

struct VS_INPUT
{
    float4 position : POSITION; // 버텍스 위치
    float4 color : COLOR; // 버텍스 색상
    float2 texcoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // 변환된 화면 좌표
    float4 color : COLOR; // 전달할 색상
    float2 texcoord : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    // 위치 변환
    output.position = mul(input.position, WorldMatrix);
    output.position = mul(output.position, ViewMatrix);
    output.position = mul(output.position, ProjectionMatrix);
    
    output.color = input.color;

    output.texcoord = input.texcoord;
    
    return output;
}