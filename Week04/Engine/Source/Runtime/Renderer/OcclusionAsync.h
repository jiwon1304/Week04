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
extern UINT32 NumDisOccluded;

class UPrimitiveComponent;
class UStaticMeshComponent;
class FGraphicsDevice;

struct QueryContext
{
    ID3D11DeviceContext* Context;
    std::queue<ID3D11Query*> Queries;
    std::queue<UStaticMeshComponent*> Meshes;
    ID3D11CommandList* CommandList;
    TArray<UStaticMeshComponent*> DisOccludedMeshes;
};



// TODO: texture만 받아서 2프레임동안 할수 있도록 만들기.
class OcclusionAsync
{
public:
    ~OcclusionAsync();
    void Init(FGraphicsDevice* graphics, int numThreads = 4);
    // InComponents -> MeshesSortedByDistance -> returnValue
    void QueryAsync(
        ID3D11DepthStencilView* DepthStencilSRV, 
        TArray<UStaticMeshComponent*> InComponents);
    TArray<UStaticMeshComponent*> GetResult();

private:
    //void Prepare();
    void End();
    // inits
    FGraphicsDevice* Graphics;

    ID3D11Query* OcclusionQuery = nullptr;
    // 가져와서 여기에 그리기

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
    void RenderOccludee(UStaticMeshComponent* StaticMeshComp, ID3D11DeviceContext* Context);
    //ID3D11ShaderResourceView* DepthStencilSRV = nullptr;
    //ID3D11SamplerState* OcclusionSampler = nullptr;

    // Query
    TArray<QueryContext> QueryContexts;
    void PrepareOcclusion(ID3D11DeviceContext* Context);
    ID3D11DepthStencilView* TargetDepthStencilView;
    D3D11_QUERY_DESC queryDesc;
    std::queue<ID3D11Query*> QueryPool;
    
    void ExecuteQuery(
        const ID3D11DepthStencilView* DepthStencilSRV,
        const TArray<UStaticMeshComponent*> InComponents,
        int Start, int End, struct QueryContext& QC);

    void GetResultOcclusionQuery(struct QueryContext& QC);

    std::future<TArray<UStaticMeshComponent*>> FutureResult; // 비동기 결과 저장
    //TArray<ID3D11CommandList*> CommandLists;
    std::vector<std::future<void>> Futures;
    int NumThreads;

    //TArray<UStaticMeshComponent*> DisOccludedMeshesAsync;
    std::future<void> AsyncQuery;
    std::promise<void> Promise;
    std::mutex QueryMutex;
    std::mutex ArrayMutex;

    bool bIsQuerying = false;
    bool bIsReady = false;
    bool bIsDone = false;
    bool bIsReadyToGet = false;
    bool bIsReadyToSet = false;
    bool bIsSet = false;

    // DirectX States
private:
    void EnableWrite(ID3D11DeviceContext* Context) const;
    void DisableWrite(ID3D11DeviceContext* Context) const;
};
