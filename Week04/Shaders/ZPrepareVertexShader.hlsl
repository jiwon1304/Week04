cbuffer MVPConstant : register(b0)
{
    matrix worldViewProj;
}

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;  // 클립 공간 위치
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    
    return output;
}