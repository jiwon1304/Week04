Texture2D Textures : register(t0);
SamplerState Sampler : register(s0);

cbuffer MatrixConstants : register(b0)
{
    row_major float4x4 MVP;
    float4 UUID;
    bool isSelected;
    float3 Pad0;
};

struct FMaterial
{
    float3 DiffuseColor;
};

cbuffer MaterialConstants : register(b1)
{
    float3 DiffuseColor;
}

cbuffer FlagConstants : register(b3)
{
    bool IsGizmo;
    float3 flagPad0;
}

struct PS_INPUT
{
    float4 position : SV_POSITION; // 변환된 화면 좌표
    float4 color : COLOR; // 전달할 색상
    float2 texcoord : TEXCOORD1;
};

struct PS_OUTPUT
{
    float4 color : SV_Target0;
    float4 UUID : SV_Target1;
};

PS_OUTPUT mainPS(PS_INPUT input)
{
    PS_OUTPUT output;
    
    output.UUID = UUID;
    // 라이팅 무시
    //output.color = Textures.Sample(Sampler, input.texcoord + UVOffset);
    //return output;
    
    float3 TexColor = Textures.Sample(Sampler, input.texcoord);
    
    float3 FinalColor;
    if (IsGizmo)
        FinalColor = DiffuseColor;
    else
    {
        FinalColor = TexColor;
    }
    
    if (isSelected)
    {
        FinalColor += float3(0.2f, 0.2f, 0.0f); // 노란색 틴트로 하이라이트
    }
    
    output.color = float4(FinalColor, 1.f);
    return output;
}