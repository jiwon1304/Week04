#pragma once
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#define _TCHAR_DEFINED
#include <d3d11.h>
#include "Core/Container/String.h"
#include "Core/Container/Array.h"
#include <array>
#include <queue>

constexpr float OCCLUSION_DISTANCE_DIV = 1.1f;  // 적절한 초기값으로 초기화
constexpr int OCCLUSION_DISTANCE_BIN_NUM = 49 + 1;  // log(1.2f)^100
constexpr int OcclusionBufferSizeWidth = 256;
constexpr int OcclusionBufferSizeHeight = 256;
extern UINT32 NumDisOccluded;

class UPrimitiveComponent;
class UStaticMeshComponent;
class FGraphicsDevice;

class Occlusion
{
public:
    void Init(FGraphicsDevice* graphics);
    void Prepare();
    // InComponents -> MeshesSortedByDistance -> returnValue
    TArray<UStaticMeshComponent*> Query(TArray<UPrimitiveComponent*> InComponents);

private:
    // inits
    FGraphicsDevice* Graphics;

    ID3D11Query* OcclusionQuery = nullptr;
    ID3D11Texture2D* OcclusionTexture = nullptr;
    ID3D11DepthStencilView* OcclusionDSV = nullptr;

    ID3D11DepthStencilState* OcclusionWriteZero = nullptr;
    ID3D11DepthStencilState* OcclusionWriteAlways = nullptr;

    // sorting
    std::array<TArray<UStaticMeshComponent*>, OCCLUSION_DISTANCE_BIN_NUM> MeshesSortedByDistance;
    void SortMeshRoughly(TArray<UPrimitiveComponent*> InComponents);

    // Render
    void CreateOcclusionShader();
    ID3D11VertexShader* OcclusionVertexShader = nullptr;
    ID3D11PixelShader* OcclusionPixelShader = nullptr;

    ID3D11Buffer* OcclusionConstantBuffer = nullptr;
    ID3D11Buffer* OcclusionObjectInfoBuffer = nullptr;
    void RenderOccluder(UStaticMeshComponent* StaticMeshComp);
    void RenderOccludee(UStaticMeshComponent* StaticMeshComp);
    ID3D11ShaderResourceView* OcclusionSRV = nullptr;

    // Query
    void PrepareOcclusion();
    D3D11_QUERY_DESC queryDesc;
    std::queue<ID3D11Query*> QueryPool;
    std::queue<ID3D11Query*> Queries;
    std::queue<UStaticMeshComponent*> QueryMeshes;
};
