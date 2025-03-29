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
#include <future>
#include <thread>

constexpr float OCCLUSION_Async_DISTANCE_DIV = 1.1f;  // 적절한 초기값으로 초기화
constexpr int OCCLUSION_Async_DISTANCE_BIN_NUM = 49 + 1;  // log(1.2f)^100
constexpr int Occlusion_ASync_BufferSizeWidth = 1024;
constexpr int Occlusion_ASync_BufferSizeHeight = 1024;
extern UINT32 NumDisOccluded;

class UPrimitiveComponent;
class UStaticMeshComponent;
class FGraphicsDevice;

// TODO: texture만 받아서 2프레임동안 할수 있도록 만들기.
class OcclusionAsync
{
public:
    void Init(FGraphicsDevice* graphics);
    // InComponents -> MeshesSortedByDistance -> returnValue
    TArray<UStaticMeshComponent*> Query(ID3D11DepthStencilView* DepthStencilSRV, TArray<UPrimitiveComponent*> InComponents);
    void QueryAsync(ID3D11ShaderResourceView* DepthStencilSRV, TArray<UPrimitiveComponent*> InComponents);
    bool Get(TArray<UStaticMeshComponent*> OutComponents);

private:
    //void Prepare();
    void End();
    // inits
    FGraphicsDevice* Graphics;

    ID3D11Query* OcclusionQuery = nullptr;
    // 가져와서 여기에 그리기
    ID3D11DepthStencilView* OcclusionDSV = nullptr;

    ID3D11DepthStencilState* OcclusionWriteZero = nullptr;
    ID3D11DepthStencilState* OcclusionWriteAlways = nullptr;

    // sorting
    std::array<TArray<UStaticMeshComponent*>, OCCLUSION_Async_DISTANCE_BIN_NUM> MeshesSortedByDistance;

    // Render
    void CreateOcclusionShader();
    ID3D11VertexShader* OcclusionVertexShader = nullptr;
    ID3D11PixelShader* OcclusionPixelShader = nullptr;

    ID3D11Buffer* OcclusionConstantBuffer = nullptr;
    ID3D11Buffer* OcclusionObjectInfoBuffer = nullptr;
    void RenderOccludee(UStaticMeshComponent* StaticMeshComp);
    //ID3D11ShaderResourceView* DepthStencilSRV = nullptr;
    //ID3D11SamplerState* OcclusionSampler = nullptr;

    // Query
    void PrepareOcclusion();
    D3D11_QUERY_DESC queryDesc;
    std::queue<ID3D11Query*> QueryPool;
    std::queue<ID3D11Query*> Queries;
    std::queue<UStaticMeshComponent*> QueryMeshes;

    // Async
    TArray<UStaticMeshComponent*> DisOccludedMeshesAsync;
    std::future<void> AsyncQuery;
    std::promise<void> Promise;
    std::mutex Mutex;

    bool bIsQuerying = false;
    bool bIsReady = false;
    bool bIsDone = false;
    bool bIsReadyToGet = false;
    bool bIsReadyToSet = false;
    bool bIsSet = false;
};
