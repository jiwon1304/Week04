#include "Occlusion.h"

#include <d3dcompiler.h>
#include "Components/StaticMeshComponent.h"
#include "Launch/EngineLoop.h"
#include "World.h"
#include "LevelEditor/SLevelEditor.h"
#include "UObject/Casts.h"
#include "D3D11RHI/GraphicDevice.h"

extern LARGE_INTEGER CPUTime;
extern LARGE_INTEGER GPUTime;
extern float CPUElapsedTime;
extern float GPUElapsedTime;
extern LARGE_INTEGER TempTime;
extern LARGE_INTEGER Frequency;
extern float GPUQueryTime;

void Occlusion::Init(FGraphicsDevice* graphics)
{
    Graphics = graphics;

    D3D11_TEXTURE2D_DESC DepthStencilBufferDesc = {};
    DepthStencilBufferDesc.Width = 1024;
    DepthStencilBufferDesc.Height = 1024;
    DepthStencilBufferDesc.MipLevels = 1;
    DepthStencilBufferDesc.ArraySize = 1;
    DepthStencilBufferDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    DepthStencilBufferDesc.SampleDesc.Count = 1;
    DepthStencilBufferDesc.SampleDesc.Quality = 0;
    DepthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    DepthStencilBufferDesc.CPUAccessFlags = 0;
    DepthStencilBufferDesc.MiscFlags = 0;

    HRESULT hr = Graphics->Device->CreateTexture2D(&DepthStencilBufferDesc, nullptr, &OcclusionTexture);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create Occlusion Texture!" << std::endl;
        return;
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc = {};
    DepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DepthStencilViewDesc.Texture2D.MipSlice = 0;

    hr = Graphics->Device->CreateDepthStencilView(OcclusionTexture, &DepthStencilViewDesc, &OcclusionDSV);

    D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
    depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSRVDesc.Texture2D.MostDetailedMip = 0;
    depthSRVDesc.Texture2D.MipLevels = 1;

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

    hr = Graphics->Device->CreateShaderResourceView(OcclusionTexture, &depthSRVDesc, &OcclusionSRV);

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

    CreateOcclusionShader();
}


void Occlusion::CreateOcclusionShader()
{
    ID3DBlob* VertexShaderCSO;
    ID3DBlob* PixelShaderCSO;

    HRESULT hr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompileFromFile(L"Shaders/DepthOnlyShader.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
    }
    hr = Graphics->Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &OcclusionVertexShader);

    hr = D3DCompileFromFile(L"Shaders/DepthOnlyShader.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, &errorBlob);
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

}


void Occlusion::Prepare()
{
    // 크기만 가져와서 rasterizer에서 사용하도록 만듦.
    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = 1024;
    Viewport.Height = 1024;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Graphics->DeviceContext->RSSetViewports(1, &Viewport);


    Graphics->DeviceContext->VSSetShader(OcclusionVertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(OcclusionPixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr); // 쉐이더 내부에서 사용.
    //Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &OcclusionConstantBuffer);
    //Graphics->DeviceContext->IASetIndexBuffer(nullptr,DXGI_FORMAT,0);

    Graphics->DeviceContext->ClearDepthStencilView(OcclusionDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState, 0);
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, OcclusionDSV); // 컬러 버퍼 끄기

}

TArray<UStaticMeshComponent*> Occlusion::Query(TArray<UPrimitiveComponent*> InComponents)
{
    // InComponents -> MeshesSortedByDistance
    SortMeshRoughly(InComponents);

    PrepareOcclusion();
    TArray<UStaticMeshComponent*> DisOccludedMeshes;
    //return;
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

    // 가까운거부터
    int count = 0;
    for (auto& Meshes : MeshesSortedByDistance)
    {
        Graphics->DeviceContext->OMSetDepthStencilState(OcclusionWriteAlways, 0);
        while (!Queries.empty())
        {
            ID3D11Query* Query = Queries.front();
            UStaticMeshComponent* StaticMeshComp = QueryMeshes.front();
            UINT64 pixelCount = 0;
            while (Graphics->DeviceContext->GetData(Query, &pixelCount, sizeof(pixelCount), 0) == S_FALSE) {}
            if (pixelCount > 0) {
                DisOccludedMeshes.Add(StaticMeshComp);
                RenderOccluder(StaticMeshComp);
            }
            Queries.pop();
            QueryMeshes.pop();
            QueryPool.push(Query);
        }

        Graphics->DeviceContext->OMSetDepthStencilState(OcclusionWriteZero, 0);
        for (auto& StaticMeshComp : Meshes)
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
    NumDisOccluded = DisOccludedMeshes.Num();

    return DisOccludedMeshes;
}




void Occlusion::SortMeshRoughly(TArray<UPrimitiveComponent*> InComponents)
{
    //std::array<UStaticMeshComponent*, DISTANCE_BIN_NUM> bins;
    for (auto& bin : MeshesSortedByDistance)
    {
        bin.Empty();
    }
    FVector CameraPos = GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->ViewTransformPerspective.GetLocation();
    for (UPrimitiveComponent* PrimComp : InComponents) {
        if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimComp))
        {
            FVector Disp = MeshComp->GetWorldLocation() - CameraPos;
            float dist = Disp.Magnitude();
            dist = log(dist) / log(OCCLUSION_DISTANCE_DIV);
            dist = dist < 0 ? 0 : dist;
            int binIndex = std::min(int(dist), OCCLUSION_DISTANCE_BIN_NUM - 1);
            MeshesSortedByDistance[binIndex].Add(MeshComp);
        }
    }
}


void Occlusion::RenderOccluder(UStaticMeshComponent* StaticMeshComp)
{
    if (!GEngineLoop.GetLevelEditor()->GetActiveViewportClient()) return;

    FVector ActorLocation = StaticMeshComp->GetWorldLocation();

    float OccluderSize = StaticMeshComp->GetStaticMesh()->GetRenderData()->OccluderRadius;

    //Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &OcclusionObjectInfoBuffer); // GridParameters (b1)
    //Graphics->DeviceContext->VSSetConstantBuffers(1, 1, &OcclusionConstantBuffer);     // MatrixBuffer (b0)

    D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
    Graphics->DeviceContext->Map(OcclusionObjectInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
    {
        FOcclusionInformation* constants = static_cast<FOcclusionInformation*>(ConstantBufferMSR.pData);
        constants->Position = ActorLocation;
        constants->Radius = OccluderSize * 1.2;
    }
    Graphics->DeviceContext->Unmap(OcclusionObjectInfoBuffer, 0);

    Graphics->DeviceContext->Draw(30, 0);
}


void Occlusion::RenderOccludee(UStaticMeshComponent* StaticMeshComp)
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

void Occlusion::PrepareOcclusion()
{
    // 크기만 가져와서 rasterizer에서 사용하도록 만듦.
    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0;
    Viewport.TopLeftY = 0;
    Viewport.Width = 1024;
    Viewport.Height = 1024;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Graphics->DeviceContext->RSSetViewports(1, &Viewport);


    Graphics->DeviceContext->VSSetShader(OcclusionVertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(OcclusionPixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(nullptr); // 쉐이더 내부에서 사용.

    Graphics->DeviceContext->ClearDepthStencilView(OcclusionDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState, 0);
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, OcclusionDSV); // 컬러 버퍼 끄기
}

