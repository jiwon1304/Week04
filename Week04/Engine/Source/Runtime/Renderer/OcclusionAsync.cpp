#include "OcclusionAsync.h"

#include <d3dcompiler.h>
#include "Components/StaticMeshComponent.h"
#include "Launch/EngineLoop.h"
#include "World.h"
#include "LevelEditor/SLevelEditor.h"
#include "UObject/Casts.h"
#include "D3D11RHI/GraphicDevice.h"
#include <future>

extern UINT32 NumDisOccluded;
extern LARGE_INTEGER CPUTime;
extern LARGE_INTEGER GPUTime;
extern float CPUElapsedTime;
extern float GPUElapsedTime;
extern LARGE_INTEGER TempTime;
extern LARGE_INTEGER Frequency;
extern float GPUQueryTime;

OcclusionAsync::~OcclusionAsync()
{
    // 최대 2초간 기다림
    for (auto& Future : Futures)
    {
        Future.wait_for(std::chrono::seconds(2));
    }
    std::lock_guard<std::mutex> Lock(QueryMutex);
    while (!QueryPool.empty())
    {
        if (QueryPool.front())
        {
            ID3D11Query* f = QueryPool.front();
            f->Release();
            QueryPool.front() = nullptr;
        }
        QueryPool.pop();
    }
    for (auto& QC : QueryContexts)
    {
        QC.Context->Flush();
        while (!QC.Queries.empty())
        {
            if (QC.Queries.front())
            {
                QC.Queries.front()->Release();
                QC.Queries.front() = nullptr;
            }
            QC.Queries.pop();
        }
        if (QC.CommandList) QC.CommandList->Release();
        if (QC.Context) QC.Context->Release();
    }
    if (OcclusionWriteZero) { 
        OcclusionWriteZero->Release(); 
        OcclusionWriteZero = nullptr;
    }
    if (OcclusionWriteAlways) {
        OcclusionWriteAlways->Release();
        OcclusionWriteAlways = nullptr;
    }
    if (OcclusionVertexShader) {
        OcclusionVertexShader->Release();
        OcclusionVertexShader = nullptr;
    }
    if (OcclusionPixelShader) {
        OcclusionPixelShader->Release();
        OcclusionPixelShader = nullptr;
    }
    if (OcclusionConstantBuffer) {
        OcclusionConstantBuffer->Release();
        OcclusionConstantBuffer = nullptr;
    }
    if (OcclusionObjectInfoBuffer) {
        OcclusionObjectInfoBuffer->Release();
        OcclusionObjectInfoBuffer = nullptr;
    }

}

void OcclusionAsync::Init(FGraphicsDevice* graphics, int numThreads)
{
    Graphics = graphics;
    this->NumThreads = numThreads;
    HRESULT hr = S_OK;
    for (int i = 0; i < numThreads; ++i)
    {
        ID3D11DeviceContext* Deferred;
        hr = Graphics->Device->CreateDeferredContext(0, &Deferred);
        if (FAILED(hr))
        {
            std::cerr << "Failed to create Deferred Device Context for Occlusion!" << std::endl;
            continue;
        }
        QueryContexts.Add({ Deferred, });
    }

    queryDesc = {};
    queryDesc.Query = D3D11_QUERY_OCCLUSION; // Occlusion Query 사용
    queryDesc.MiscFlags = 0;

    const int numQueries = 50000;
    for (int i = 0; i < numQueries; ++i)
    {
        ID3D11Query* Query;
        HRESULT hr = Graphics->Device->CreateQuery(&queryDesc, &Query);
        if (FAILED(hr))
        {
            std::cerr << "Failed to create Occlusion Query!" << std::endl;
            continue;
        }
        QueryPool.push(Query);
    }

    
    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
    DepthStencilDesc.DepthEnable = TRUE;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DepthStencilDesc.StencilEnable = FALSE; // Stencil Test 비활성화
    Graphics->Device->CreateDepthStencilState(&DepthStencilDesc, &OcclusionWriteAlways);

    DepthStencilDesc.DepthEnable = TRUE;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    Graphics->Device->CreateDepthStencilState(&DepthStencilDesc, &OcclusionWriteZero);

    CreateOcclusionShader();
}


void OcclusionAsync::CreateOcclusionShader()
{
    ID3DBlob* VertexShaderCSO;
    ID3DBlob* PixelShaderCSO;

    HRESULT hr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompileFromFile(L"Shaders/DepthOnlyShader.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &errorBlob);

    hr = Graphics->Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &OcclusionVertexShader);

    hr = D3DCompileFromFile(L"Shaders/DepthOnlyShader.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
    }
    hr = Graphics->Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &OcclusionPixelShader);

    VertexShaderCSO->Release();
    PixelShaderCSO->Release();

    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FDepthOnlyShaderConstants) + 0xf & 0xfffffff0;
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &OcclusionConstantBuffer);

    constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FOcclusionInformation) + 0xf & 0xfffffff0;
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &OcclusionObjectInfoBuffer);

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    //Graphics->Device->CreateSamplerState(&samplerDesc, &OcclusionSampler);

}

void OcclusionAsync::End()
{
    for (auto& ContextQuery : QueryContexts)
    {
        EnableWrite(ContextQuery.Context);
        //ContextQuery.Context->ClearDepthStencilView(TargetDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
}

void OcclusionAsync::RenderOccludee(UStaticMeshComponent* StaticMeshComp, ID3D11DeviceContext* Context)
{
    FVector ActorLocation = StaticMeshComp->GetWorldLocation();

    float OccludeeSize = StaticMeshComp->GetStaticMesh()->GetRenderData()->OccludeeRadius;

    D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
    Context->Map(OcclusionObjectInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
    {
        FOcclusionInformation* constants = static_cast<FOcclusionInformation*>(ConstantBufferMSR.pData);
        constants->Position = ActorLocation;
        constants->Radius = OccludeeSize * 1.5;
    }
    Context->Unmap(OcclusionObjectInfoBuffer, 0);

    Context->Draw(30, 0);
}

void OcclusionAsync::PrepareOcclusion(ID3D11DeviceContext* Context, ID3D11DepthStencilView* TargetDepthStencilView)
{
    // 크기만 가져와서 rasterizer에서 사용하도록 만듦.
    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = Graphics->screenWidth;
    Viewport.Height = Graphics->screenHeight;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &Viewport);

    Context->VSSetShader(OcclusionVertexShader, nullptr, 0);
    Context->PSSetShader(OcclusionPixelShader, nullptr, 0);
    Context->IASetInputLayout(nullptr); // 쉐이더 내부에서 사용.
        
    Context->VSSetConstantBuffers(0, 1, &OcclusionObjectInfoBuffer); // GridParameters (b1)
    Context->VSSetConstantBuffers(1, 1, &OcclusionConstantBuffer);     // MatrixBuffer (b0)

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->OMSetRenderTargets(0, nullptr, TargetDepthStencilView); // 이전 프레임의 깊이 정보를 사용함.
    DisableWrite(Context);

    
}

// 비동기 오클루전 컬링 실행 (결과는 나중에 GetResult()로 받음)
// thread는 init때만 정함.
void OcclusionAsync::QueryAsync(
    ID3D11DepthStencilView* InDepthStencilView,
    TArray<UStaticMeshComponent*> InComponents)
{
    Futures.clear();
    
    int TotalComponents = InComponents.Len();
    int ChunkSize = (TotalComponents + NumThreads - 1) / NumThreads;

    assert(NumThreads == QueryContexts.Num());

    FViewportCameraTransform CameraTransform = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->ViewTransformPerspective;

    FVector CameraLocation = CameraTransform.GetLocation();
    FMatrix ViewProjection = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetViewMatrix()
        * GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetProjectionMatrix();


    for (int i = 0; i < NumThreads; ++i)
    {
        int Start = i * ChunkSize;
        int End = FMath::Min((i + 1) * ChunkSize, TotalComponents);
        if (Start >= End) break;

        // read only니깐 상관없음
        Futures.push_back(std::async(std::launch::async, &OcclusionAsync::ExecuteQuery, this, 
            InDepthStencilView, InComponents, Start, End , ViewProjection, CameraLocation, std::ref(QueryContexts[i])));
    }

    // 모든 쓰레드가 drawcall을 끝낼때까지 기다림.
    // 실제로는 command queue에 넣고있긴함.
    // TODO : futre 하나 끝나면 하나 다 execute하기
    for (auto& Future : Futures)
    {
        Future.wait();
    }


    for (auto& QC : QueryContexts)
    {
        // command list에 모두 push.
        ID3D11CommandList* CommandList = nullptr;
        HRESULT hr = QC.Context->FinishCommandList(FALSE, &CommandList);
        if (FAILED(hr)) {
            hr = Graphics->Device->GetDeviceRemovedReason();
            std::cerr << "Failed to create Command List!" << std::endl;
        }
        QC.CommandList = CommandList;

    }

    // Command list execute
    for (auto& QC : QueryContexts)
    {
        Graphics->DeviceContext->ExecuteCommandList(QC.CommandList, true);
        QC.CommandList->Release();
        QC.CommandList = nullptr;
    }
}

// 실제 쓰레드별 내부에서 작동하는 함수
void OcclusionAsync::ExecuteQuery(ID3D11DepthStencilView* DepthStencilSRV, 
    const TArray<UStaticMeshComponent*> InComponents, int Start, int End,
    const FMatrix ViewProjection, const FVector CameraPos,
    struct QueryContext& QC)
{
    PrepareOcclusion(QC.Context, DepthStencilSRV);
    //TArray<UStaticMeshComponent*> LocalDisOccludedMeshes
    QC.DisOccludedMeshes.Empty();
    D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
    QC.Context->Map(this->OcclusionConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
    {
        FDepthOnlyShaderConstants* constants = static_cast<FDepthOnlyShaderConstants*>(ConstantBufferMSR.pData);
        constants->ViewProjection = ViewProjection;
        constants->CameraPos = CameraPos;
    }
    QC.Context->Unmap(this->OcclusionConstantBuffer, 0);

    for (int i = Start; i < End; ++i)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(InComponents[i]))
        {
            //if ((CameraPos).Distance(StaticMeshComp->GetWorldLocation()) < 1.f)
            if (StaticMeshComp->bWasDrawnBefore)
            {
                QC.DisOccludedMeshes.Add(StaticMeshComp);
                StaticMeshComp->bAlreadyQueued = true;
                StaticMeshComp->bWasDrawnBefore = true;
            }


            ID3D11Query* Query = nullptr;
            {
                // Query Pool에서 Lock을 하면 안될거같음...
                std::lock_guard<std::mutex> Lock(this->QueryMutex);
                if (!QueryPool.empty())
                {
                    Query = QueryPool.front();
                    QueryPool.pop();
                }
            }

            if (!Query)
            {
                D3D11_QUERY_DESC queryDesc = {};
                queryDesc.Query = D3D11_QUERY_OCCLUSION;
                HRESULT hr = Graphics->Device->CreateQuery(&queryDesc, &Query);
                if (FAILED(hr))
                {
                    std::cerr << "Failed to create Occlusion Query!" << std::endl;
                    continue;
                }
            }

            QC.Context->Begin(Query);
            // 렌더링 수행 (개별 메시)
            RenderOccludee(StaticMeshComp, QC.Context);
            QC.Context->End(Query);
            QC.Queries.push(Query);
            QC.Meshes.push(StaticMeshComp);
        }
    }

}

// 나중에 결과를 받아오기
TArray<UStaticMeshComponent*> OcclusionAsync::GetResult()
{
    TArray<UStaticMeshComponent*>DisOccludedMeshesGlobal;
    for (QueryContext &QC : QueryContexts)
    {
        GetResultOcclusionQuery(QC);
        DisOccludedMeshesGlobal + QC.DisOccludedMeshes;
        //QC.Context->ClearState();
    }
    NumDisOccluded = DisOccludedMeshesGlobal.Num();

    return DisOccludedMeshesGlobal;
}


// GetData는 무조건 immediateContext에서밖에 못함. Deferred에서 불가능.
// https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata
void OcclusionAsync::GetResultOcclusionQuery(struct QueryContext& QC)
{
    while (!QC.Queries.empty())
    {
        ID3D11Query* Query = QC.Queries.front();
        UStaticMeshComponent* StaticMeshComp = QC.Meshes.front();
        UINT64 pixelCount = 0;
        while (Graphics->DeviceContext->GetData(Query, &pixelCount, sizeof(pixelCount), 0) == S_FALSE) {}
        // 통과할경우
        if (pixelCount > 0) {
            // 이미 들어가있는 항목일경우(전에 그린거) 중복을 안하고 안넣음
            if (!StaticMeshComp->bAlreadyQueued)
            {
                QC.DisOccludedMeshes.Add(StaticMeshComp);
                StaticMeshComp->bWasDrawnBefore = true;
            }
        }
        else
        {
            StaticMeshComp->bWasDrawnBefore = false;
        }
        // 초기화
        StaticMeshComp->bAlreadyQueued = false;

        QC.Queries.pop();
        QC.Meshes.pop();

        QueryPool.push(Query);

    }
}

inline void OcclusionAsync::EnableWrite(ID3D11DeviceContext* Context) const
{
    Context->OMSetDepthStencilState(OcclusionWriteAlways, 0);
}

inline void OcclusionAsync::DisableWrite(ID3D11DeviceContext* Context) const
{
    Context->OMSetDepthStencilState(OcclusionWriteZero, 0);
}

