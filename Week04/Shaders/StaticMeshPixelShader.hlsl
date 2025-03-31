#include "ShaderBuffers.hlsl"

struct FMaterial
{
    float3 DiffuseColor;
};

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
        FinalColor += float3(0.4f, 0.4f, 0.0f); // 노란색 틴트로 하이라이트
    }

    FinalColor = FinalColor + input.color;
    
    output.color = float4(FinalColor, 1.f);
    return output;
}