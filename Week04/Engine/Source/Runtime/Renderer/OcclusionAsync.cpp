#include "OcclusionAsync.h"

#include <d3dcompiler.h>
#include "Components/StaticMeshComponent.h"
#include "Launch/EngineLoop.h"
#include "World.h"
#include "LevelEditor/SLevelEditor.h"
#include "UObject/Casts.h"
#include "D3D11RHI/GraphicDevice.h"

extern UINT32 NumDisOccluded;
extern LARGE_INTEGER CPUTime;
extern LARGE_INTEGER GPUTime;
extern float CPUElapsedTime;
extern float GPUElapsedTime;
extern LARGE_INTEGER TempTime;
extern LARGE_INTEGER Frequency;
extern float GPUQueryTime;

void OcclusionAsync::Init(FGraphicsDevice* graphics)
{
    Graphics = graphics;

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

//
//void OcclusionAsync::Prepare()
//{
//
//
//}

void OcclusionAsync::End()
{
    Graphics->DeviceContext->OMSetDepthStencilState(OcclusionWriteAlways, 0);
    Graphics->DeviceContext->ClearDepthStencilView(OcclusionDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

TArray<UStaticMeshComponent*> OcclusionAsync::Query(ID3D11DepthStencilView* DepthStencilSRV, TArray<UPrimitiveComponent*> InComponents)
{
    OcclusionDSV = DepthStencilSRV;
    //Prepare();
    PrepareOcclusion();

    TArray<UStaticMeshComponent*> DisOccludedMeshes;
    FViewportCameraTransform CameraTransform = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->ViewTransformPerspective;

    FVector CameraLocation = CameraTransform.GetLocation();
    FMatrix VP = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetViewMatrix() * GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetProjectionMatrix();

    D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
    Graphics->DeviceContext->Map(OcclusionConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
    {
        FDepthOnlyShaderConstants* constants = static_cast<FDepthOnlyShaderConstants*>(ConstantBufferMSR.pData);
        constants->ViewProjection = VP;
        constants->CameraPos = CameraLocation;
    }
    Graphics->DeviceContext->Unmap(OcclusionConstantBuffer, 0);

    Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &OcclusionObjectInfoBuffer); // GridParameters (b1)
    Graphics->DeviceContext->VSSetConstantBuffers(1, 1, &OcclusionConstantBuffer);     // MatrixBuffer (b0)

    Graphics->DeviceContext->OMSetDepthStencilState(OcclusionWriteZero, 0);

    for (auto& Meshes : InComponents)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Meshes))
        {
            ID3D11Query* Query;
            if (!QueryPool.empty())
            {
                Query = QueryPool.front();
                QueryPool.pop();
            }
            else
            {
                HRESULT hr = Graphics->Device->CreateQuery(&queryDesc, &Query);
                if (FAILED(hr))
                {
                    std::cerr << "Failed to create Occlusion Query!" << std::endl;
                    continue;
                }
                QueryPool.push(Query);
            }
            Graphics->DeviceContext->Begin(Query);
            RenderOccludee(StaticMeshComp);
            Graphics->DeviceContext->End(Query);
            Queries.push(Query);
            QueryMeshes.push(StaticMeshComp);
        }
    }

    while (!Queries.empty())
    {
        ID3D11Query* Query = Queries.front();
        UStaticMeshComponent* StaticMeshComp = QueryMeshes.front();
        UINT64 pixelCount = 0;
        while (Graphics->DeviceContext->GetData(Query, &pixelCount, sizeof(pixelCount), 0) == S_FALSE) {}
        if (pixelCount > 0) {
            DisOccludedMeshes.Add(StaticMeshComp);
        }
        Queries.pop();
        QueryMeshes.pop();
        QueryPool.push(Query);
    }
    End();
    NumDisOccluded = DisOccludedMeshes.Len();
    return DisOccludedMeshes;
}

// 아직 안씀!!!
void OcclusionAsync::QueryAsync(ID3D11ShaderResourceView* DepthStencilSRV, TArray<UPrimitiveComponent*> InComponents)
{
    // InComponents -> MeshesSortedByDistance
    PrepareOcclusion();

    TArray<UStaticMeshComponent*> DisOccludedMeshes;
    FViewportCameraTransform CameraTransform = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->ViewTransformPerspective;

    FVector CameraLocation = CameraTransform.GetLocation();
    FMatrix VP = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetViewMatrix() * GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->GetProjectionMatrix();

    D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
    Graphics->DeviceContext->Map(OcclusionConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
    {
        FDepthOnlyShaderConstants* constants = static_cast<FDepthOnlyShaderConstants*>(ConstantBufferMSR.pData);
        constants->ViewProjection = VP;
        constants->CameraPos = CameraLocation;
    }
    Graphics->DeviceContext->Unmap(OcclusionConstantBuffer, 0);

    Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &OcclusionObjectInfoBuffer); // GridParameters (b1)
    Graphics->DeviceContext->VSSetConstantBuffers(1, 1, &OcclusionConstantBuffer);     // MatrixBuffer (b0)

    Graphics->DeviceContext->OMSetDepthStencilState(OcclusionWriteZero, 0);

    for (auto& Meshes : InComponents)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Meshes))
        {
            ID3D11Query* Query;
            if (!QueryPool.empty())
            {
                Query = QueryPool.front();
                QueryPool.pop();
            }
            else
            {
                break;
                HRESULT hr = Graphics->Device->CreateQuery(&queryDesc, &Query);
                if (FAILED(hr))
                {
                    std::cerr << "Failed to create Occlusion Query!" << std::endl;
                    continue;
                }
                QueryPool.push(Query);
            }
            Graphics->DeviceContext->Begin(Query);
            RenderOccludee(StaticMeshComp);
            Graphics->DeviceContext->End(Query);
            Queries.push(Query);
            QueryMeshes.push(StaticMeshComp);
        }
           
    }

    while (!Queries.empty())
    {
        ID3D11Query* Query = Queries.front();
        UStaticMeshComponent* StaticMeshComp = QueryMeshes.front();
        UINT64 pixelCount = 0;
        while (Graphics->DeviceContext->GetData(Query, &pixelCount, sizeof(pixelCount), 0) == S_FALSE) {}
        if (pixelCount > 0) {
            DisOccludedMeshes.Add(StaticMeshComp);
        }
        Queries.pop();
        QueryMeshes.pop();
        QueryPool.push(Query);
    }

    NumDisOccluded = DisOccludedMeshes.Num();

    //return DisOccludedMeshes;
}



void OcclusionAsync::RenderOccludee(UStaticMeshComponent* StaticMeshComp)
{
    if (!GEngineLoop.GetLevelEditor()->GetActiveViewportClient()) return;

    FVector ActorLocation = StaticMeshComp->GetWorldLocation();

    float OccludeeSize = StaticMeshComp->GetStaticMesh()->GetRenderData()->OccludeeRadius;

    //Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &OcclusionObjectInfoBuffer); // GridParameters (b1)
    //Graphics->DeviceContext->VSSetConstantBuffers(1, 1, &OcclusionConstantBuffer);     // MatrixBuffer (b0)

    D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
    Graphics->DeviceContext->Map(OcclusionObjectInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
    {
        FOcclusionInformation* constants = static_cast<FOcclusionInformation*>(ConstantBufferMSR.pData);
        constants->Position = ActorLocation;
        constants->Radius = OccludeeSize;
    }
    Graphics->DeviceContext->Unmap(OcclusionObjectInfoBuffer, 0);

    Graphics->DeviceContext->Draw(30, 0);
}

void OcclusionAsync::PrepareOcclusion()
{
    // 크기만 가져와서 rasterizer에서 사용하도록 만듦.
    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = Occlusion_ASync_BufferSizeWidth;
    Viewport.Height = Occlusion_ASync_BufferSizeHeight;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Graphics->DeviceContext->RSSetViewports(1, &Viewport);

    Graphics->DeviceContext->VSSetShader(OcclusionVertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(OcclusionPixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr); // 쉐이더 내부에서 사용.

    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState, 0);
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, OcclusionDSV); // 컬러 버퍼 끄기
    Graphics->DeviceContext->OMSetDepthStencilState(OcclusionWriteZero, 0);

    //Graphics->DeviceContext->PSSetShaderResources(0, 1, &DepthStencilSRV);
    //Graphics->DeviceContext->PSSetSamplers(0, 1, &OcclusionSampler);
}

