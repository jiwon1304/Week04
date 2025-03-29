// Z-prepass용 버텍스 셰이더
cbuffer cbPerObject : register(b0)
{
    matrix worldViewProj; // 월드-뷰-투영 변환 행렬
}

struct VS_INPUT
{
    float4 position : POSITION; // 버텍스 위치
    float4 color : COLOR; // 버텍스 색상
    float3 normal : NORMAL; // 버텍스 노멀
    float2 texcoord : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;  // 클립 공간 위치
};

VS_OUTPUT mainVS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // 위치 변환만 수행
    output.position = mul(input.position, worldViewProj);
    
    return output;
}