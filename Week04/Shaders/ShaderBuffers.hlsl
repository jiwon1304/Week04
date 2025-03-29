
#ifndef COMMON_BUFFERS_INCLUDED
#define COMMON_BUFFERS_INCLUDED

Texture2D Textures : register(t0);

SamplerState Sampler : register(s0);

cbuffer MatrixConstants : register(b0)
{
    row_major matrix WorldMatrix;
    float4 UUID;
    bool isSelected;
    float3 Pad0;
};

cbuffer MaterialConstants : register(b1)
{
    float3 DiffuseColor;
};

cbuffer FlagConstants : register(b3)
{
    bool IsGizmo;
    float3 flagPad0;
};

cbuffer ViewMatrix : register(b5)
{
    row_major matrix ViewMatrix;
};

cbuffer ViewMatrix : register(b6)
{
    row_major matrix ProjectionMatrix;
};

cbuffer GridParametersData : register(b7)
{
    float GridSpacing;
    int GridCount; // 총 grid 라인 수
    float3 GridOrigin; // Grid의 중심
    float Padding;
};

cbuffer PrimitiveCounts : register(b13) // Not used
{
    int BoundingBoxCount; // 렌더링할 AABB의 개수
    int pad;
    int ConeCount; // 렌더링할 cone의 개수
    int pad1;
};

#endif
