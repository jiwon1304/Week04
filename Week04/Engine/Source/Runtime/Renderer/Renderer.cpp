#include "Renderer.h"
#include <d3dcompiler.h>

#include "World.h"
#include "Actors/Player.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/UBillboardComponent.h"
#include "Components/UParticleSubUVComp.h"
#include "Components/UText.h"
#include "Components/Material/Material.h"
#include "D3D11RHI/GraphicDevice.h"
#include "Launch/EngineLoop.h"
#include "Math/JungleMath.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealEd/PrimitiveBatch.h"
#include "UObject/Casts.h"
#include "UObject/Object.h"
#include "PropertyEditor/ShowFlags.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkySphereComponent.h"
#include "OctreeNode.h"
#include "BaseGizmos/TransformGizmo.h"
#include "UObject/UObjectIterator.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include <future>

void FRenderer::Initialize(FGraphicsDevice* graphics)
{
    Graphics = graphics;
    CreateShader();
    CreateTextureShader();
    CreateLineShader();
    CreateConstantBuffer();
    CreateLightingBuffer();
    CreateLitUnlitBuffer();
    CreateUAV();
    CreateQuad();

    BindBuffers();
}

void FRenderer::BindBuffers()
{
    Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    Graphics->DeviceContext->VSSetConstantBuffers(5, 1, &ConstantBufferView);
    Graphics->DeviceContext->VSSetConstantBuffers(6, 1, &ConstantBufferProjection);
    Graphics->DeviceContext->VSSetConstantBuffers(7, 1, &GridConstantBuffer);
    Graphics->DeviceContext->VSSetConstantBuffers(13, 1, &LinePrimitiveBuffer);
        
    Graphics->DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer);
    Graphics->DeviceContext->PSSetConstantBuffers(1, 1, &MaterialConstantBuffer);
    Graphics->DeviceContext->PSSetConstantBuffers(3, 1, &FlagBuffer);
    Graphics->DeviceContext->PSSetConstantBuffers(7, 1, &GridConstantBuffer);
}

void FRenderer::SetViewport(std::shared_ptr<FEditorViewportClient> InActiveViewport)
{
    ActiveViewport = InActiveViewport;
    Graphics->DeviceContext->RSSetViewports(1, &ActiveViewport->GetD3DViewport());
}

void FRenderer::SetWorld(UWorld* InWorld)
{
    World = InWorld;

    // Octree별로 bake
    FOctreeNode* WorldOctree = World->GetOctree();
    constexpr UINT32 MaxBatchNum = 64;

    //TArray<TArray<UStaticMeshComponent*>> Batches = AggregateComponents(WorldOctree, MaxBatchNum);

}

TArray<TArray<UStaticMeshComponent*>> FRenderer::AggregateMeshComponents(FOctreeNode* Octree, uint32 MaxAggregateNum = 64)
{
    return TArray<TArray<UStaticMeshComponent*>>();
    //if (Octree->CountAllComponents() <= MaxAggregateNum)
    //{
    //    TArray<TArray<UStaticMeshComponent*>> SortedByMesh;
    //    for (UPrimitiveComponent* Comp : Octree->Components)
    //    {
    //        if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Comp))
    //        {
    //            if (SortedByMesh.Num() == 0) // 첫번째는 무조건 넣기
    //            {
    //                SortedByMesh.Add({ MeshComp });
    //            }
    //            else // 두번째 이후로는 비교하면서 넣기
    //            {
    //                bool bIsSameMaterialExist = false;
    //                for (TArray<UStaticMeshComponent*> Materials : SortedByMesh)
    //                {
    //                    if(Materials[0]->GetStaticMesh()->GetMaterials()-> )
    //                }
    //            }
    //        }


    //    }

    //    return TArray<TArray<UStaticMeshComponent*>>();
    //}
    //else
    //{
    //    TArray<TArray<UStaticMeshComponent*>> Batches;
    //    for (auto& SubOctree : Octree->Children)
    //    {
    //        if (SubOctree)
    //        {
    //            TArray<TArray<UStaticMeshComponent*>> SubBatches = AggregateMeshComponents(SubOctree.get(), MaxAggregateNum);
    //            for (auto Batch : Batches)
    //            {
    //                Batches += SubBatches;
    //            }
    //        }
    //    }
    //    return Batches;
    //}
}

void FRenderer::SortByMaterial(TArray<UPrimitiveComponent*> PrimComps)
{
    //uint32 NumThreads = std::thread::hardware_concurrency() * 2; // big-little구조에선 잘 작동안함.
    //if (NumThreads == 0) NumThreads = 8;
    uint32 NumThreads = 8; 

    TArray <std::future< 
        std::unordered_map<UMaterial*,
        std::unordered_map<UStaticMesh*, std::vector<FRenderer::FMeshData>>
        >>>Futures;

    int ChunkSize = PrimComps.Num() / (NumThreads - 1);
    if (PrimComps.Num() < 128) ChunkSize = PrimComps.Num();
    else if (PrimComps.Num() < 256) ChunkSize = PrimComps.Num()/2;
    else if (PrimComps.Num() < 512) ChunkSize = PrimComps.Num()/4;
    for (int i = 0; i < NumThreads; ++i)
    {
        int Start = i * ChunkSize;
        int End = FMath::Min((i + 1) * ChunkSize, PrimComps.Num());
        if (Start >= End) break;

        // read only니깐 상관없음
        Futures.Add(std::async(std::launch::async, &FRenderer::SortByMaterialThread, this,
            std::ref(PrimComps), Start, End));
    }

    // 모든 future 결과를 기다리기
    for (auto& Future : Futures)
    {
        auto ChunkResult = Future.get(); // 결과 받아오기

        // 반환된 ChunkResult를 기존 MaterialMeshMap에 병합
        for (auto& [Material, DataMap] : ChunkResult)
        {
            for (auto& [StaticMesh, DataArray] : DataMap)
            {
                // MaterialMeshMap에 이미 존재하는 키들에 데이터를 덧붙이기
                MaterialMeshMap[Material][StaticMesh].insert(
                    MaterialMeshMap[Material][StaticMesh].end(),
                    DataArray.begin(),
                    DataArray.end()
                );
            }
        }
    }

}

std::unordered_map<UMaterial*, std::unordered_map<UStaticMesh*, std::vector<FRenderer::FMeshData>>> 
    FRenderer::SortByMaterialThread(TArray<UPrimitiveComponent*>& PrimComps, uint32 start, uint32 end)
{
    AActor* SelectedActor = World->GetSelectedActor();
    UTransformGizmo* GizmoActor = World->LocalGizmo;

    std::unordered_map<UMaterial*, std::unordered_map<UStaticMesh*, std::vector<FMeshData>>> MaterialMeshMapLocal;
    for(int i = start; i < end; ++i)
    {
        if (GizmoActor->GetComponents().Contains(PrimComps[i]))
        {
            // 기즈모는 Frustum 컬링이 적용되지 않게 따로 관리할 예정이므로 여기에서는 건너뜀.
        }
        // UGizmoBaseComponent가 UStaticMeshComponent를 상속받으므로, 정확히 구분하기 위함.
        else if (UStaticMeshComponent* pStaticMeshComp = Cast<UStaticMeshComponent>(PrimComps[i]))
        {
            UStaticMesh* StaticMesh = pStaticMeshComp->GetStaticMesh();
            if (!StaticMesh)
            {
                continue;
            }

            int SubMeshIdx = 0;

            FMeshData Data;
            Data.WorldMatrix = pStaticMeshComp->GetWorldMatrix();
            Data.bIsSelected = SelectedActor == pStaticMeshComp->GetOwner();

            for (const auto& subMesh : pStaticMeshComp->GetStaticMesh()->GetRenderData()->MaterialSubsets)
            {
                UMaterial* Material = pStaticMeshComp->GetStaticMesh()->GetMaterials()[0]->Material;
                Data.IndexStart = subMesh.IndexStart;
                Data.IndexCount = subMesh.IndexCount;
                MaterialMeshMapLocal[Material][StaticMesh].push_back(Data);
                SubMeshIdx++;
            }
        }
    }

    return MaterialMeshMapLocal;
}

void FRenderer::Release()
{
    ReleaseShader();
    ReleaseTextureShader();
    ReleaseLineShader();
    ReleaseConstantBuffer();
    ReleaseUAV();
    ReleaseQuad();
}

void FRenderer::CreateShader()
{
    ID3DBlob* VertexShaderCSO;
    ID3DBlob* PixelShaderCSO;

    D3DCompileFromFile(L"Shaders/StaticMeshVertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, nullptr);
    Graphics->Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &VertexShader);

    D3DCompileFromFile(L"Shaders/StaticMeshPixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, nullptr);
    Graphics->Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &PixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Graphics->Device->CreateInputLayout(
        layout, ARRAYSIZE(layout), VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &InputLayout
    );

    Stride = sizeof(FVertexSimple);
    VertexShaderCSO->Release();
    PixelShaderCSO->Release();
}

void FRenderer::ReleaseShader()
{
    if (InputLayout)
    {
        InputLayout->Release();
        InputLayout = nullptr;
    }
    if (PixelShader)
    {
        PixelShader->Release();
        PixelShader = nullptr;
    }
    if (VertexShader)
    {
        VertexShader->Release();
        VertexShader = nullptr;
    }
}

void FRenderer::PrepareShader() const
{
    Graphics->DeviceContext->VSSetShader(VertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PixelShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
}

void FRenderer::PrepareShaderDeferred(ID3D11DeviceContext* Context) const
{
        Context->VSSetShader(VertexShader, nullptr, 0);
        Context->PSSetShader(PixelShader, nullptr, 0);
        Context->IASetInputLayout(InputLayout);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FRenderer::ResetVertexShader() const
{
    Graphics->DeviceContext->VSSetShader(nullptr, nullptr, 0);
    VertexShader->Release();
}

void FRenderer::ResetPixelShader() const
{
    Graphics->DeviceContext->PSSetShader(nullptr, nullptr, 0);
    PixelShader->Release();
}

void FRenderer::SetVertexShader(const FWString& filename, const FString& funcname, const FString& version)
{
    // ���� �߻��� ���ɼ��� ����
    if (Graphics == nullptr)
        assert(0);
    if (VertexShader != nullptr)
        ResetVertexShader();
    if (InputLayout != nullptr)
        InputLayout->Release();
    ID3DBlob* vertexshaderCSO;

    D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, *funcname, *version, 0, 0, &vertexshaderCSO, nullptr);
    Graphics->Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &VertexShader);
    vertexshaderCSO->Release();
}

void FRenderer::SetPixelShader(const FWString& filename, const FString& funcname, const FString& version)
{
    // ���� �߻��� ���ɼ��� ����
    if (Graphics == nullptr)
        assert(0);
    if (VertexShader != nullptr)
        ResetVertexShader();
    ID3DBlob* pixelshaderCSO;
    D3DCompileFromFile(filename.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, *funcname, *version, 0, 0, &pixelshaderCSO, nullptr);
    Graphics->Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &PixelShader);

    pixelshaderCSO->Release();
}

void FRenderer::ChangeViewMode(EViewModeIndex evi) const
{
    /* W04 
    switch (evi)
    {
    case EViewModeIndex::VMI_Lit:
        UpdateLitUnlitConstant(1);
        break;
    case EViewModeIndex::VMI_Wireframe:
    case EViewModeIndex::VMI_Unlit:
        UpdateLitUnlitConstant(0);
        break;
    }
    */
}

void FRenderer::RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices) const
{
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
    Graphics->DeviceContext->Draw(numVertices, 0);
}

void FRenderer::RenderPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices) const
{
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Stride, &offset);
    Graphics->DeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    Graphics->DeviceContext->DrawIndexed(numIndices, 0, 0);
}

void FRenderer::RenderPrimitive(OBJ::FStaticMeshRenderData* renderData, TArray<FStaticMaterial*> materials, TArray<UMaterial*> overrideMaterial, int selectedSubMeshIndex = -1) const
{
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &renderData->VertexBuffer, &Stride, &offset);

    if (renderData->IndexBuffer)
        Graphics->DeviceContext->IASetIndexBuffer(renderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    if (renderData->MaterialSubsets.Num() == 0)
    {
        // no submesh
        Graphics->DeviceContext->DrawIndexed(renderData->Indices.Num(), 0, 0);
    }

    for (int subMeshIndex = 0; subMeshIndex < renderData->MaterialSubsets.Num(); subMeshIndex++)
    {
        int materialIndex = renderData->MaterialSubsets[subMeshIndex].MaterialIndex;

        subMeshIndex == selectedSubMeshIndex ? UpdateSubMeshConstant(true) : UpdateSubMeshConstant(false);

        overrideMaterial[materialIndex] != nullptr ? 
            UpdateMaterial(overrideMaterial[materialIndex]->GetMaterialInfo()) : UpdateMaterial(materials[materialIndex]->Material->GetMaterialInfo());

        if (renderData->IndexBuffer)
        {
            // index draw
            uint64 startIndex = renderData->MaterialSubsets[subMeshIndex].IndexStart;
            uint64 indexCount = renderData->MaterialSubsets[subMeshIndex].IndexCount;
            Graphics->DeviceContext->DrawIndexed(indexCount, startIndex, 0);
        }
    }
}

void FRenderer::RenderTexturedModelPrimitive(
    ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices, ID3D11ShaderResourceView* InTextureSRV,
    ID3D11SamplerState* InSamplerState
) const
{
    if (!InTextureSRV || !InSamplerState)
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "SRV, Sampler Error");
    }
    if (numIndices <= 0)
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "numIndices Error");
    }
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &Stride, &offset);
    Graphics->DeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    Graphics->DeviceContext->PSSetShaderResources(0, 1, &InTextureSRV);
    Graphics->DeviceContext->PSSetSamplers(0, 1, &InSamplerState);

    Graphics->DeviceContext->DrawIndexed(numIndices, 0, 0);
}

ID3D11Buffer* FRenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth) const
{
    // 2. Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};

    ID3D11Buffer* vertexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "VertexBuffer Creation faild");
    }
    return vertexBuffer;
}

ID3D11Buffer* FRenderer::CreateVertexBuffer(const TArray<FVertexSimple>& vertices, UINT byteWidth) const
{
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD;
    vertexbufferSRD.pSysMem = vertices.GetData();

    ID3D11Buffer* vertexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "VertexBuffer Creation faild");
    }
    return vertexBuffer;
}

ID3D11Buffer* FRenderer::CreateVertexBuffer(const TArray<FVector>& vertices, UINT byteWidth) const
{
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD;
    vertexbufferSRD.pSysMem = vertices.GetData();

    ID3D11Buffer* vertexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "VertexBuffer Creation faild");
    }
    return vertexBuffer;
}

ID3D11Buffer* FRenderer::CreateIndexBuffer(uint32* indices, UINT byteWidth) const
{
    D3D11_BUFFER_DESC indexbufferdesc = {};              // buffer�� ����, �뵵 ���� ����
    indexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;       // immutable: gpu�� �б� �������� ������ �� �ִ�.
    indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // index buffer�� ����ϰڴ�.
    indexbufferdesc.ByteWidth = byteWidth;               // buffer ũ�� ����

    D3D11_SUBRESOURCE_DATA indexbufferSRD = {indices};

    ID3D11Buffer* indexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&indexbufferdesc, &indexbufferSRD, &indexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "IndexBuffer Creation faild");
    }
    return indexBuffer;
}

ID3D11Buffer* FRenderer::CreateIndexBuffer(const TArray<uint32>& indices, UINT byteWidth) const
{
    D3D11_BUFFER_DESC indexbufferdesc = {};              // buffer�� ����, �뵵 ���� ����
    indexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;       // immutable: gpu�� �б� �������� ������ �� �ִ�.
    indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // index buffer�� ����ϰڴ�.
    indexbufferdesc.ByteWidth = byteWidth;               // buffer ũ�� ����

    D3D11_SUBRESOURCE_DATA indexbufferSRD;
    indexbufferSRD.pSysMem = indices.GetData();

    ID3D11Buffer* indexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&indexbufferdesc, &indexbufferSRD, &indexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "IndexBuffer Creation faild");
    }
    return indexBuffer;
}

void FRenderer::ReleaseBuffer(ID3D11Buffer*& Buffer) const
{
    if (Buffer)
    {
        Buffer->Release();
        Buffer = nullptr;
    }
}

void FRenderer::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0;
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);

    constantbufferdesc.ByteWidth = sizeof(FConstantsView) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBufferView);

    constantbufferdesc.ByteWidth = sizeof(FConstantsProjection) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBufferProjection);

    constantbufferdesc.ByteWidth = sizeof(FSubUVConstant) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &SubUVConstantBuffer);

    constantbufferdesc.ByteWidth = sizeof(FGridParameters) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &GridConstantBuffer);

    constantbufferdesc.ByteWidth = sizeof(FPrimitiveCounts) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &LinePrimitiveBuffer);

    constantbufferdesc.ByteWidth = sizeof(FMaterialConstants) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &MaterialConstantBuffer);
    
    constantbufferdesc.ByteWidth = sizeof(FSubMeshConstants) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &SubMeshConstantBuffer);

    constantbufferdesc.ByteWidth = sizeof(FTextureConstants) + 0xf & 0xfffffff0;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &TextureConstantBuffer);
}

void FRenderer::CreateLightingBuffer()
{
    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FLighting);
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &LightingBuffer);
}

void FRenderer::CreateLitUnlitBuffer()
{
    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FLitUnlitConstants);
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Graphics->Device->CreateBuffer(&constantbufferdesc, nullptr, &FlagBuffer);
}

void FRenderer::ReleaseConstantBuffer()
{
    if (ConstantBuffer)
    {
        ConstantBuffer->Release();
        ConstantBuffer = nullptr;
    }

    if (ConstantBufferView)
    {
        ConstantBufferView->Release();
        ConstantBufferView = nullptr;
    }

    if (ConstantBufferProjection)
    {
        ConstantBufferProjection->Release();
        ConstantBufferProjection = nullptr;
    }

    if (LightingBuffer)
    {
        LightingBuffer->Release();
        LightingBuffer = nullptr;
    }

    if (FlagBuffer)
    {
        FlagBuffer->Release();
        FlagBuffer = nullptr;
    }

    if (MaterialConstantBuffer)
    {
        MaterialConstantBuffer->Release();
        MaterialConstantBuffer = nullptr;
    }

    if (SubMeshConstantBuffer)
    {
        SubMeshConstantBuffer->Release();
        SubMeshConstantBuffer = nullptr;
    }

    if (TextureConstantBuffer)
    {
        TextureConstantBuffer->Release();
        TextureConstantBuffer = nullptr;
    }
}

void FRenderer::UpdateLightBuffer() const
{
    if (!LightingBuffer) return;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Graphics->DeviceContext->Map(LightingBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    {
        FLighting* constants = static_cast<FLighting*>(mappedResource.pData);
        constants->lightDirX = 1.0f; // ��: ���� ������ �Ʒ��� �������� ���
        constants->lightDirY = 1.0f; // ��: ���� ������ �Ʒ��� �������� ���
        constants->lightDirZ = 1.0f; // ��: ���� ������ �Ʒ��� �������� ���
        constants->lightColorX = 1.0f;
        constants->lightColorY = 1.0f;
        constants->lightColorZ = 1.0f;
        constants->AmbientFactor = 0.06f;
    }
    Graphics->DeviceContext->Unmap(LightingBuffer, 0);
}

void FRenderer::UpdateConstant(const FMatrix& WorldMatrix, FVector4 UUIDColor, bool IsSelected) const
{
    if (ConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR; // GPU�� �޸� �ּ� ����

        Graphics->DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
        {
            FConstants* constants = static_cast<FConstants*>(ConstantBufferMSR.pData);
            constants->WorldMatrix = WorldMatrix;
            constants->UUIDColor = UUIDColor;
            constants->IsSelected = IsSelected;
        }
        Graphics->DeviceContext->Unmap(ConstantBuffer, 0); // GPU�� �ٽ� ��밡���ϰ� �����
    }
}

void FRenderer::UpdateConstantDeferred(ID3D11DeviceContext* Context, const FMatrix& WorldMatrix, FVector4 UUIDColor, bool IsSelected) const
{
    if (ConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR; // GPU�� �޸� �ּ� ����

        Context->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
        {
            FConstants* constants = static_cast<FConstants*>(ConstantBufferMSR.pData);
            constants->WorldMatrix = WorldMatrix;
            constants->UUIDColor = UUIDColor;
            constants->IsSelected = IsSelected;
        }
        Context->Unmap(ConstantBuffer, 0); // GPU�� �ٽ� ��밡���ϰ� �����
    }
}

void FRenderer::UpdateViewMatrix(const FMatrix& InViewMatrix) const
{
    if (ConstantBufferView)
    {
        D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;

        Graphics->DeviceContext->Map(ConstantBufferView, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
        {
            FConstantsView* constants = static_cast<FConstantsView*>(ConstantBufferMSR.pData);
            constants->ViewMatrix = InViewMatrix;
        }
        Graphics->DeviceContext->Unmap(ConstantBufferView, 0);
    }
}

void FRenderer::UpdateProjectionMatrix(const FMatrix& InProjectionMatrix) const
{
    if (ConstantBufferProjection)
    {
        D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;

        Graphics->DeviceContext->Map(ConstantBufferProjection, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
        {
            FConstantsProjection* constants = static_cast<FConstantsProjection*>(ConstantBufferMSR.pData);
            constants->ProjectionMatrix = InProjectionMatrix;
        }
        Graphics->DeviceContext->Unmap(ConstantBufferProjection, 0);
    }
}

void FRenderer::UpdateMaterial(const FObjMaterialInfo& MaterialInfo) const
{
    if (MaterialConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;

        Graphics->DeviceContext->Map(MaterialConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
        {
            FMaterialConstants* constants = static_cast<FMaterialConstants*>(ConstantBufferMSR.pData);
            constants->DiffuseColor = MaterialInfo.Diffuse;
        }
        Graphics->DeviceContext->Unmap(MaterialConstantBuffer, 0);
    }

    if (MaterialInfo.bHasTexture == true)
    {
        std::shared_ptr<FTexture> texture = FEngineLoop::ResourceManager.GetTexture(MaterialInfo.DiffuseTexturePath);
        Graphics->DeviceContext->PSSetShaderResources(0, 1, &texture->TextureSRV);
        Graphics->DeviceContext->PSSetSamplers(0, 1, &texture->SamplerState);
    }
    else
    {
        ID3D11ShaderResourceView* nullSRV[1] = {nullptr};
        ID3D11SamplerState* nullSampler[1] = {nullptr};

        Graphics->DeviceContext->PSSetShaderResources(0, 1, nullSRV);
        Graphics->DeviceContext->PSSetSamplers(0, 1, nullSampler);
    }
}

void FRenderer::UpdateMaterialDeferred(ID3D11DeviceContext* Context, const FObjMaterialInfo& MaterialInfo) const
{
    if (MaterialConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;

        Context->Map(MaterialConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
        {
            FMaterialConstants* constants = static_cast<FMaterialConstants*>(ConstantBufferMSR.pData);
            constants->DiffuseColor = MaterialInfo.Diffuse;
        }
        Context->Unmap(MaterialConstantBuffer, 0);
    }

    if (MaterialInfo.bHasTexture == true)
    {
        std::shared_ptr<FTexture> texture = FEngineLoop::ResourceManager.GetTexture(MaterialInfo.DiffuseTexturePath);
        Context->PSSetShaderResources(0, 1, &texture->TextureSRV);
        Context->PSSetSamplers(0, 1, &texture->SamplerState);
    }
    else
    {
        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        ID3D11SamplerState* nullSampler[1] = { nullptr };

        Context->PSSetShaderResources(0, 1, nullSRV);
        Context->PSSetSamplers(0, 1, nullSampler);
    }
}

void FRenderer::UpdateLitUnlitConstant(int isLit) const
{
    if (FlagBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU �� �޸� �ּ� ����
        Graphics->DeviceContext->Map(FlagBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
        auto constants = static_cast<FLitUnlitConstants*>(constantbufferMSR.pData); //GPU �޸� ���� ����
        {
            constants->isLit = isLit;
        }
        Graphics->DeviceContext->Unmap(FlagBuffer, 0);
    }
}

void FRenderer::UpdateIsGizmoConstant(int IsGizmo) const
{
    // W04 - 위 함수와 동일한 상수 버퍼를 다룸. 이번에는 IsLit을 IsGizmo로 용도를 변경하여 메시 렌더링 시 디퓨즈 맵과 컬러를 선택
    if (FlagBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
        Graphics->DeviceContext->Map(FlagBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
        auto constants = static_cast<FLitUnlitConstants*>(constantbufferMSR.pData);
        {
            constants->isLit = IsGizmo;
        }
        Graphics->DeviceContext->Unmap(FlagBuffer, 0);
    }
}

void FRenderer::UpdateSubMeshConstant(bool isSelected) const
{
    if (SubMeshConstantBuffer) {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU �� �޸� �ּ� ����
        Graphics->DeviceContext->Map(SubMeshConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
        FSubMeshConstants* constants = (FSubMeshConstants*)constantbufferMSR.pData; //GPU �޸� ���� ����
        {
            constants->isSelectedSubMesh = isSelected;
        }
        Graphics->DeviceContext->Unmap(SubMeshConstantBuffer, 0);
    }
}

void FRenderer::UpdateTextureConstant(float UOffset, float VOffset)
{
    if (TextureConstantBuffer) {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU �� �޸� �ּ� ����
        Graphics->DeviceContext->Map(TextureConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
        FTextureConstants* constants = (FTextureConstants*)constantbufferMSR.pData; //GPU �޸� ���� ����
        {
            constants->UOffset = UOffset;
            constants->VOffset = VOffset;
        }
        Graphics->DeviceContext->Unmap(TextureConstantBuffer, 0);
    }
}

void FRenderer::CreateTextureShader()
{
    ID3DBlob* vertextextureshaderCSO;
    ID3DBlob* pixeltextureshaderCSO;

    HRESULT hr;
    hr = D3DCompileFromFile(L"Shaders/VertexTextureShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vertextextureshaderCSO, nullptr);
    if (FAILED(hr))
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "VertexShader Error");
    }
    Graphics->Device->CreateVertexShader(
        vertextextureshaderCSO->GetBufferPointer(), vertextextureshaderCSO->GetBufferSize(), nullptr, &VertexTextureShader
    );

    hr = D3DCompileFromFile(L"Shaders/PixelTextureShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &pixeltextureshaderCSO, nullptr);
    if (FAILED(hr))
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "PixelShader Error");
    }
    Graphics->Device->CreatePixelShader(
        pixeltextureshaderCSO->GetBufferPointer(), pixeltextureshaderCSO->GetBufferSize(), nullptr, &PixelTextureShader
    );

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    Graphics->Device->CreateInputLayout(
        layout, ARRAYSIZE(layout), vertextextureshaderCSO->GetBufferPointer(), vertextextureshaderCSO->GetBufferSize(), &TextureInputLayout
    );

    //�ڷᱸ�� ���� �ʿ�
    TextureStride = sizeof(FVertexTexture);
    vertextextureshaderCSO->Release();
    pixeltextureshaderCSO->Release();
}

void FRenderer::ReleaseTextureShader()
{
    if (TextureInputLayout)
    {
        TextureInputLayout->Release();
        TextureInputLayout = nullptr;
    }

    if (PixelTextureShader)
    {
        PixelTextureShader->Release();
        PixelTextureShader = nullptr;
    }

    if (VertexTextureShader)
    {
        VertexTextureShader->Release();
        VertexTextureShader = nullptr;
    }
    if (SubUVConstantBuffer)
    {
        SubUVConstantBuffer->Release();
        SubUVConstantBuffer = nullptr;
    }
    if (ConstantBuffer)
    {
        ConstantBuffer->Release();
        ConstantBuffer = nullptr;
    }
}

void FRenderer::PrepareTextureShader() const
{
    Graphics->DeviceContext->VSSetShader(VertexTextureShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PixelTextureShader, nullptr, 0);
    Graphics->DeviceContext->IASetInputLayout(TextureInputLayout);

    //�ؽ��Ŀ� ConstantBuffer �߰��ʿ��Ҽ���
    if (ConstantBuffer)
    {
        Graphics->DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    }
}

ID3D11Buffer* FRenderer::CreateVertexTextureBuffer(FVertexTexture* vertices, UINT byteWidth) const
{
    // 2. Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    //D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

    ID3D11Buffer* vertexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&vertexbufferdesc, nullptr, &vertexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "VertexBuffer Creation faild");
    }
    return vertexBuffer;
}

ID3D11Buffer* FRenderer::CreateIndexTextureBuffer(uint32* indices, UINT byteWidth) const
{
    D3D11_BUFFER_DESC indexbufferdesc = {};
    indexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexbufferdesc.ByteWidth = byteWidth;
    indexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* indexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&indexbufferdesc, nullptr, &indexBuffer);
    if (FAILED(hr))
    {
        return nullptr;
    }
    return indexBuffer;
}

void FRenderer::RenderTexturePrimitive(
    ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices, ID3D11ShaderResourceView* _TextureSRV,
    ID3D11SamplerState* _SamplerState
) const
{
    if (!_TextureSRV || !_SamplerState)
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "SRV, Sampler Error");
    }
    if (numIndices <= 0)
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "numIndices Error");
    }
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &TextureStride, &offset);
    Graphics->DeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Graphics->DeviceContext->PSSetShaderResources(0, 1, &_TextureSRV);
    Graphics->DeviceContext->PSSetSamplers(0, 1, &_SamplerState);

    Graphics->DeviceContext->DrawIndexed(numIndices, 0, 0);
}

//��Ʈ ��ġ������
void FRenderer::RenderTextPrimitive(
    ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11ShaderResourceView* _TextureSRV, ID3D11SamplerState* _SamplerState
) const
{
    if (!_TextureSRV || !_SamplerState)
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "SRV, Sampler Error");
    }
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &TextureStride, &offset);

    // �Է� ���̾ƿ� �� �⺻ ����
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Graphics->DeviceContext->PSSetShaderResources(0, 1, &_TextureSRV);
    Graphics->DeviceContext->PSSetSamplers(0, 1, &_SamplerState);

    // ��ο� ȣ�� (6���� �ε��� ���)
    Graphics->DeviceContext->Draw(numVertices, 0);
}


ID3D11Buffer* FRenderer::CreateVertexBuffer(FVertexTexture* vertices, UINT byteWidth) const
{
    // 2. Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};

    ID3D11Buffer* vertexBuffer;

    HRESULT hr = Graphics->Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);
    if (FAILED(hr))
    {
        UE_LOG(LogLevel::Warning, "VertexBuffer Creation faild");
    }
    return vertexBuffer;
}

void FRenderer::UpdateSubUVConstant(float _indexU, float _indexV) const
{
    if (SubUVConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR; // GPU�� �޸� �ּ� ����

        Graphics->DeviceContext->Map(SubUVConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
        auto constants = static_cast<FSubUVConstant*>(constantbufferMSR.pData);                               //GPU �޸� ���� ����
        {
            constants->indexU = _indexU;
            constants->indexV = _indexV;
        }
        Graphics->DeviceContext->Unmap(SubUVConstantBuffer, 0); // GPU�� �ٽ� ��밡���ϰ� �����
    }
}

void FRenderer::PrepareSubUVConstant() const
{
    if (SubUVConstantBuffer && false) // Not this time
    {
        Graphics->DeviceContext->VSSetConstantBuffers(1, 1, &SubUVConstantBuffer);
        Graphics->DeviceContext->PSSetConstantBuffers(1, 1, &SubUVConstantBuffer);
    }
}

void FRenderer::PrepareLineShader() const
{
    // ���̴��� �Է� ���̾ƿ� ����
    Graphics->DeviceContext->VSSetShader(VertexLineShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(PixelLineShader, nullptr, 0);
}

void FRenderer::CreateLineShader()
{
    ID3DBlob* VertexShaderLine;
    ID3DBlob* PixelShaderLine;

    HRESULT hr;
    hr = D3DCompileFromFile(L"Shaders/ShaderLine.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainVS", "vs_5_0", 0, 0, &VertexShaderLine, nullptr);
    if (FAILED(hr))
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "VertexShader Error");
    }
    Graphics->Device->CreateVertexShader(VertexShaderLine->GetBufferPointer(), VertexShaderLine->GetBufferSize(), nullptr, &VertexLineShader);

    hr = D3DCompileFromFile(L"Shaders/ShaderLine.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", 0, 0, &PixelShaderLine, nullptr);
    if (FAILED(hr))
    {
        Console::GetInstance().AddLog(LogLevel::Warning, "PixelShader Error");
    }
    Graphics->Device->CreatePixelShader(PixelShaderLine->GetBufferPointer(), PixelShaderLine->GetBufferSize(), nullptr, &PixelLineShader);

    VertexShaderLine->Release();
    PixelShaderLine->Release();
}

void FRenderer::ReleaseLineShader() const
{
    if (GridConstantBuffer) GridConstantBuffer->Release();
    if (LinePrimitiveBuffer) LinePrimitiveBuffer->Release();
    if (VertexLineShader) VertexLineShader->Release();
    if (PixelLineShader) PixelLineShader->Release();
}

ID3D11Buffer* FRenderer::CreateStaticVerticesBuffer() const
{
    FSimpleVertex vertices[2]{{0}, {0}};

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA vbInitData = {};
    vbInitData.pSysMem = vertices;
    ID3D11Buffer* pVertexBuffer = nullptr;
    HRESULT hr = Graphics->Device->CreateBuffer(&vbDesc, &vbInitData, &pVertexBuffer);
    return pVertexBuffer;
}

ID3D11Buffer* FRenderer::CreateBoundingBoxBuffer(UINT numBoundingBoxes) const
{
    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC; // ���� ������Ʈ�� ��� DYNAMIC, �׷��� ������ DEFAULT
    bufferDesc.ByteWidth = sizeof(FBoundingBox) * numBoundingBoxes;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(FBoundingBox);

    ID3D11Buffer* BoundingBoxBuffer = nullptr;
    Graphics->Device->CreateBuffer(&bufferDesc, nullptr, &BoundingBoxBuffer);
    return BoundingBoxBuffer;
}

ID3D11Buffer* FRenderer::CreateOBBBuffer(UINT numBoundingBoxes) const
{
    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC; // ���� ������Ʈ�� ��� DYNAMIC, �׷��� ������ DEFAULT
    bufferDesc.ByteWidth = sizeof(FOBB) * numBoundingBoxes;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(FOBB);

    ID3D11Buffer* BoundingBoxBuffer = nullptr;
    Graphics->Device->CreateBuffer(&bufferDesc, nullptr, &BoundingBoxBuffer);
    return BoundingBoxBuffer;
}

ID3D11Buffer* FRenderer::CreateConeBuffer(UINT numCones) const
{
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(FCone) * numCones;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(FCone);

    ID3D11Buffer* ConeBuffer = nullptr;
    Graphics->Device->CreateBuffer(&bufferDesc, nullptr, &ConeBuffer);
    return ConeBuffer;
}

ID3D11ShaderResourceView* FRenderer::CreateBoundingBoxSRV(ID3D11Buffer* pBoundingBoxBuffer, UINT numBoundingBoxes)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // ����ü ������ ��� UNKNOWN
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.ElementOffset = 0;
    srvDesc.Buffer.NumElements = numBoundingBoxes;


    Graphics->Device->CreateShaderResourceView(pBoundingBoxBuffer, &srvDesc, &pBBSRV);
    return pBBSRV;
}

ID3D11ShaderResourceView* FRenderer::CreateOBBSRV(ID3D11Buffer* pBoundingBoxBuffer, UINT numBoundingBoxes)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // ����ü ������ ��� UNKNOWN
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.ElementOffset = 0;
    srvDesc.Buffer.NumElements = numBoundingBoxes;
    Graphics->Device->CreateShaderResourceView(pBoundingBoxBuffer, &srvDesc, &pOBBSRV);
    return pOBBSRV;
}

ID3D11ShaderResourceView* FRenderer::CreateConeSRV(ID3D11Buffer* pConeBuffer, UINT numCones)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // ����ü ������ ��� UNKNOWN
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.ElementOffset = 0;
    srvDesc.Buffer.NumElements = numCones;


    Graphics->Device->CreateShaderResourceView(pConeBuffer, &srvDesc, &pConeSRV);
    return pConeSRV;
}

void FRenderer::UpdateBoundingBoxBuffer(ID3D11Buffer* pBoundingBoxBuffer, const TArray<FBoundingBox>& BoundingBoxes, int numBoundingBoxes) const
{
    if (!pBoundingBoxBuffer) return;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Graphics->DeviceContext->Map(pBoundingBoxBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    auto pData = reinterpret_cast<FBoundingBox*>(mappedResource.pData);
    for (int i = 0; i < BoundingBoxes.Num(); ++i)
    {
        pData[i] = BoundingBoxes[i];
    }
    Graphics->DeviceContext->Unmap(pBoundingBoxBuffer, 0);
}

void FRenderer::UpdateOBBBuffer(ID3D11Buffer* pBoundingBoxBuffer, const TArray<FOBB>& BoundingBoxes, int numBoundingBoxes) const
{
    if (!pBoundingBoxBuffer) return;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Graphics->DeviceContext->Map(pBoundingBoxBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    auto pData = reinterpret_cast<FOBB*>(mappedResource.pData);
    for (int i = 0; i < BoundingBoxes.Num(); ++i)
    {
        pData[i] = BoundingBoxes[i];
    }
    Graphics->DeviceContext->Unmap(pBoundingBoxBuffer, 0);
}

void FRenderer::UpdateConesBuffer(ID3D11Buffer* pConeBuffer, const TArray<FCone>& Cones, int numCones) const
{
    if (!pConeBuffer) return;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Graphics->DeviceContext->Map(pConeBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    auto pData = reinterpret_cast<FCone*>(mappedResource.pData);
    for (int i = 0; i < Cones.Num(); ++i)
    {
        pData[i] = Cones[i];
    }
    Graphics->DeviceContext->Unmap(pConeBuffer, 0);
}

void FRenderer::UpdateGridConstantBuffer(const FGridParameters& gridParams) const
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = Graphics->DeviceContext->Map(GridConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &gridParams, sizeof(FGridParameters));
        Graphics->DeviceContext->Unmap(GridConstantBuffer, 0);
    }
    else
    {
        UE_LOG(LogLevel::Warning, "gridParams ���� ����");
    }
}

void FRenderer::UpdateLinePrimitveCountBuffer(int numBoundingBoxes, int numCones) const
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = Graphics->DeviceContext->Map(LinePrimitiveBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    auto pData = static_cast<FPrimitiveCounts*>(mappedResource.pData);
    pData->BoundingBoxCount = numBoundingBoxes;
    pData->ConeCount = numCones;
    Graphics->DeviceContext->Unmap(LinePrimitiveBuffer, 0);
}

void FRenderer::RenderBatch(
    const FGridParameters& gridParam, ID3D11Buffer* pVertexBuffer, int boundingBoxCount, int coneCount, int coneSegmentCount, int obbCount
) const
{
    UINT stride = sizeof(FSimpleVertex);
    UINT offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    UINT vertexCountPerInstance = 2;
    UINT instanceCount = gridParam.numGridLines + 3 + (boundingBoxCount * 12) + (coneCount * (2 * coneSegmentCount)) + (12 * obbCount);
    Graphics->DeviceContext->DrawInstanced(vertexCountPerInstance, instanceCount, 0, 0);
    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void FRenderer::PrepareRender(bool bShouldUpdateRender)
{
    if (!bShouldUpdateRender)
    {
        return;
    }

    // 렌더할 컴포넌트를 담기 전에 배열 비움
    ClearRenderArr();
    
    Frustum Frustum = ActiveViewport->GetFrustum();
    FOctreeNode* Octree = World->GetOctree();

    TArray<UPrimitiveComponent*> Components;
    Octree->FrustumCull(Frustum, Components);

    AActor* SelectedActor = World->GetSelectedActor();
    UTransformGizmo* GizmoActor = World->LocalGizmo;
    
    DiscardByUUID(Components, Components);


    // Setup
    Graphics->DeviceContext->OMSetRenderTargets(1, &QuadRTV, Graphics->DepthStencilView);
    Graphics->DeviceContext->ClearRenderTargetView(QuadRTV, ClearColor);
    Graphics->DeviceContext->ClearDepthStencilView(Graphics->DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    Graphics->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Graphics->DeviceContext->IASetInputLayout(InputLayout);
    Graphics->DeviceContext->OMSetDepthStencilState(Graphics->DepthStencilState, 0);
    // End Setup

    for (const auto& Comp : Components)
    {
        if (GizmoActor->GetComponents().Contains(Comp))
        {
            // 기즈모는 Frustum 컬링이 적용되지 않게 따로 관리할 예정이므로 여기에서는 건너뜀.
        }
        // UGizmoBaseComponent가 UStaticMeshComponent를 상속받으므로, 정확히 구분하기 위함.
        else if (UStaticMeshComponent* pStaticMeshComp = Cast<UStaticMeshComponent>(Comp))
        {
            UStaticMesh* StaticMesh = pStaticMeshComp->GetStaticMesh();
            if (!StaticMesh)
            {
                continue;
            }
            
            int SubMeshIdx = 0;
            
            FMeshData Data;
            Data.WorldMatrix = pStaticMeshComp->GetWorldMatrix();
            Data.bIsSelected = SelectedActor == pStaticMeshComp->GetOwner();
            
            for (const FMaterialSubset& SubMesh : pStaticMeshComp->GetStaticMesh()->GetRenderData()->MaterialSubsets)
            {
                UMaterial* Material = StaticMesh->GetMaterials()[SubMesh.MaterialIndex]->Material;
                Data.IndexStart = SubMesh.IndexStart;
                Data.IndexCount = SubMesh.IndexCount;
                MaterialMeshMap[Material][StaticMesh].push_back(Data);
                SubMeshIdx++;
            }
        }
    }
    
    if (SelectedActor)
    {
        ControlMode CM = World->GetEditorPlayer()->GetControlMode();
        const TArray<UStaticMeshComponent*>& CompArr =
            (CM == ControlMode::CM_TRANSLATION) ? GizmoActor->GetArrowArr() :
            (CM == ControlMode::CM_ROTATION)    ? GizmoActor->GetDiscArr()  :
                                                  GizmoActor->GetScaleArr();

        for (const auto& Comp : CompArr)
        {
            GizmoObjs.Add(Cast<UGizmoBaseComponent>(Comp));
        }
    }
}

bool FRenderer::IsInsideFrustum(UStaticMeshComponent* StaticMeshComp) const
{
    Frustum Frustum = ActiveViewport->GetFrustum();
    FVector Location = StaticMeshComp->GetWorldLocation();
    FBoundingBox AABB = StaticMeshComp->LocalAABB;
    AABB.min = AABB.min + Location;
    AABB.max = AABB.max + Location;
    /*TArray<FVector> vertices = aabb.GetVertices();
    for (const FVector& vertex : vertices) {
        for (int i = 0; i < 6; i++) {
            Plane& plane = frustum.planes[i];
            float dist = vertex.Dot(plane.normal) + plane.d;
            if (dist < 0) {
                bIsInsideFrustum = false;
                break;
            }
        }
        if (!bIsInsideFrustum)
            break;
    }*/
    for (int i = 0; i < 6; ++i)
    {
        const Plane& Plane = Frustum.planes[i];
        FVector PositiveVector = AABB.GetPositiveVertex(Plane.normal);
        float Dist = PositiveVector.Dot(Plane.normal) + Plane.d;
        if (Dist < 0)
        {
            return false;
        }
    }
    return true;
}

void FRenderer::ClearRenderArr()
{
    // SortedStaticMeshObjs.clear();
    for (auto& [Material, DataMap] : MaterialMeshMap)
    {
        for (auto& [StaticMesh, DataArray] : DataMap)
        {
            // W04 - 이번 게임잼 특성상 사용되는 Material과 스태틱메시가 고정되어있으므로, 맵에서 Key를 삭제하지는 않음.
            DataArray.clear();
        }
    }
    GizmoObjs.Empty();
    //BillboardObjs.Empty();
    //LightObjs.Empty();
}

void FRenderer::Render()
{
    // Graphics->DeviceContext->RSSetViewports(1, &ActiveViewport->GetD3DViewport()); // W04 - Init에서 진행
    
    // Graphics->ChangeRasterizer(VMI_Lit);
    
    // ChangeViewMode(ActiveViewport->GetViewMode()); // W04
    
    // UpdateLightBuffer(); // W04

    // TODO: W04 - 아래 함수는 월드 그리드와 바운딩 박스를 렌더함. 그리드와 바운딩 박스 렌더 분리해야함.
    UPrimitiveBatch::GetInstance().RenderBatch();

    UpdateIsGizmoConstant(0);
    RenderStaticMeshes();

    UpdateIsGizmoConstant(1);
    RenderGizmos();

    /* W04
    if (ActiveViewport->GetShowFlag() & static_cast<uint64>(EEngineShowFlags::SF_BillboardText))
        RenderBillboards(World, ActiveViewport); 
    
    RenderLight(World, ActiveViewport);
    */
}

void FRenderer::RenderStaticMeshes()
{
    PrepareShader();
    int NumThreadsRedering = 0;

    ID3D11CommandList* CommandList[NUM_DEFERRED_CONTEXT];
    for (auto& [Material, DataMap] : MaterialMeshMap)
    {
        // 이번에 사용하는 머티리얼을 GPU로 전달
        UpdateMaterial(Material->GetMaterialInfo());
        for (const auto& [StaticMesh, DataArray] : DataMap)
        {
            if (DataArray.size() == 0) continue;
            // Create a vector to store threads
            TArray<std::thread> threads;

            // Split the DataArray into chunks and process each chunk in a separate thread
            size_t chunk_size = DataArray.size() / (NUM_DEFERRED_CONTEXT-1);  // Divide by number of hardware threads
            
            if (chunk_size < 512)
            {
                chunk_size = DataArray.size();
            }
            for (size_t i = 0; i < DataArray.size(); i += chunk_size)
            {
                // Calculate the end index of the chunk
                size_t end = std::min(i + chunk_size, DataArray.size());

                // Create a lambda that processes each chunk of FMeshData
                size_t tid = i / chunk_size;
                threads.Add(std::thread([this, &DataArray, i, end, tid, Material, StaticMesh, &CommandList]()
                    {
                    RenderStaticMeshesThread(DataArray, i, end, tid, Material, StaticMesh, CommandList[tid]);
                    }));
                NumThreadsRedering++;
            }
            // Wait for all threads to finish for the current StaticMesh
            for (auto& t : threads)
            {
                t.join();
            }
        }
    }

    for (int i = 0; i < NumThreadsRedering; i++)
    {
        Graphics->DeferredContexts[i]->FinishCommandList(FALSE, &CommandList[i]);
    }

    // Command list execute
    for (int i = 0; i < NumThreadsRedering; i++)
    {
        Graphics->DeviceContext->ExecuteCommandList(CommandList[i], true);
        CommandList[i]->Release();
        CommandList[i] = nullptr;
    }
}

void FRenderer::RenderGizmos()
{
    if (!World->GetSelectedActor())
    {
        return;
    }

    #pragma region GizmoDepth
        ID3D11DepthStencilState* DepthStateDisable = Graphics->DepthStateDisable;
        Graphics->DeviceContext->OMSetDepthStencilState(DepthStateDisable, 0);
    #pragma endregion GizmoDepth

    //  fill solid,  Wirframe 에서도 제대로 렌더링되기 위함. W04 - 레스터라이저 생성 시 설정해주고 있음.
    // Graphics->DeviceContext->RSSetState(FEngineLoop::GraphicDevice.RasterizerStateSOLID);
    
    for (auto GizmoComp : GizmoObjs)
    {
        if (!GizmoComp->GetStaticMesh()) continue;
        OBJ::FStaticMeshRenderData* renderData = GizmoComp->GetStaticMesh()->GetRenderData();
        if (renderData == nullptr) continue;

        // 기즈모는 예외로 매트릭스 계산해서 가져옴. 구현상 한계 때문.
        FMatrix Model = JungleMath::CreateModelMatrix(GizmoComp->GetWorldLocation(),
            GizmoComp->GetWorldRotation(),
            GizmoComp->GetWorldScale()
        );
        FVector4 UUIDColor = GizmoComp->EncodeUUID() / 255.0f;
        FMatrix WorldMatrix = Model;
        UpdateConstant(WorldMatrix, UUIDColor, GizmoComp == World->GetPickingGizmo());

        RenderPrimitive(renderData, GizmoComp->GetStaticMesh()->GetMaterials(), GizmoComp->GetOverrideMaterials());
    }

#pragma region GizmoDepth
    ID3D11DepthStencilState* originalDepthState = Graphics->DepthStencilState;
    Graphics->DeviceContext->OMSetDepthStencilState(originalDepthState, 0);
#pragma endregion GizmoDepth
}

void FRenderer::RenderBillboards()
{
    PrepareTextureShader();
    PrepareSubUVConstant();
    for (auto BillboardComp : BillboardObjs)
    {
        UpdateSubUVConstant(BillboardComp->finalIndexU, BillboardComp->finalIndexV);

        FMatrix Model = BillboardComp->CreateBillboardMatrix();

        // 최종 MVP 행렬
        FMatrix MVP = Model * ActiveViewport->GetViewMatrix() * ActiveViewport->GetProjectionMatrix();
        FVector4 UUIDColor = BillboardComp->EncodeUUID() / 255.0f;
        if (BillboardComp == World->GetPickingGizmo())
            UpdateConstant(MVP, UUIDColor, true);
        else
            UpdateConstant(MVP, UUIDColor, false);

        if (UParticleSubUVComp* SubUVParticle = Cast<UParticleSubUVComp>(BillboardComp))
        {
            RenderTexturePrimitive(
                SubUVParticle->vertexSubUVBuffer, SubUVParticle->numTextVertices,
                SubUVParticle->indexTextureBuffer, SubUVParticle->numIndices, SubUVParticle->Texture->TextureSRV, SubUVParticle->Texture->SamplerState
            );
        }
        else if (UText* Text = Cast<UText>(BillboardComp))
        {
            FEngineLoop::Renderer.RenderTextPrimitive(
                Text->vertexTextBuffer, Text->numTextVertices,
                Text->Texture->TextureSRV, Text->Texture->SamplerState
            );
        }
        else
        {
            RenderTexturePrimitive(
                BillboardComp->vertexTextureBuffer, BillboardComp->numVertices,
                BillboardComp->indexTextureBuffer, BillboardComp->numIndices, BillboardComp->Texture->TextureSRV, BillboardComp->Texture->SamplerState
            );
        }
    }
    PrepareShader();
}

HRESULT FRenderer::CreateUAV()
{
    constexpr uint32 MAX_UUID_COUNT = 2 << 10; // 2^16개의 uuid만 허용. 필요하면 2^32-2까지 늘릴 수 있음.
    HRESULT hr = S_OK;
    //////////////////////////////////////////
    // UAV에 사용할 Texture 생성
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = Graphics->screenWidth;
    textureDesc.Height = Graphics->screenHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R32_UINT;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    Graphics->Device->CreateTexture2D(&textureDesc, nullptr, &UUIDMapTexture);

    // Shader Resource View (SRV) 생성
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_UINT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = Graphics->Device->CreateShaderResourceView(UUIDMapTexture, &srvDesc, &UUIDTextureSRV);
    if (FAILED(hr))
    {
        return hr;
    }
    // UAV 생성
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    hr = Graphics->Device->CreateUnorderedAccessView(UUIDMapTexture, &uavDesc, &UUIDTextureUAV);
    if (FAILED(hr))
    {
        return hr;
    }

    //////////////////////////////////////////
    // UUID 결과 저장용 Structured Buffer 생성
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.ByteWidth = sizeof(uint32) * (MAX_UUID_COUNT + 1);
    bufferDesc.StructureByteStride = sizeof(uint32);
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = Graphics->Device->CreateBuffer(&bufferDesc, nullptr, &UUIDListBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    // UAV 생성
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavBufferDesc = {};
    uavBufferDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavBufferDesc.Buffer.NumElements = MAX_UUID_COUNT + 1;
    hr = Graphics->Device->CreateUnorderedAccessView(UUIDListBuffer, &uavBufferDesc, &UUIDListUAV);
    if (FAILED(hr))
    {
        return hr;
    }

    //////////////////////////////////////////
    // shader 설정

    ID3DBlob* VertexShaderCSO;
    ID3DBlob* PixelShaderCSO;
    ID3DBlob* ComputeShaderCSO;
    ID3DBlob* ErrorBlob = nullptr;

    hr = D3DCompileFromFile(L"Shaders/UUIDRenderShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &ErrorBlob);
    if (FAILED(hr))
    {
        if (ErrorBlob) {
            OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
            ErrorBlob->Release();
        }
    }
    Graphics->Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &UUIDVertexShader);

    hr = D3DCompileFromFile(L"Shaders/UUIDRenderShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, &ErrorBlob);
    if (FAILED(hr))
    {
        if (ErrorBlob) {
            OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
            ErrorBlob->Release();
        }
    }
    Graphics->Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &UUIDPixelShader);

    hr = D3DCompileFromFile(L"Shaders/UUIDRenderShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainCS", "cs_5_0", 0, 0, &ComputeShaderCSO, &ErrorBlob);
    if (FAILED(hr))
    {
        if (ErrorBlob) {
            OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
            ErrorBlob->Release();
        }
    }
    Graphics->Device->CreateComputeShader(ComputeShaderCSO->GetBufferPointer(), ComputeShaderCSO->GetBufferSize(), nullptr, &UUIDComputeShader);


    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Graphics->Device->CreateInputLayout(
        layout, ARRAYSIZE(layout), VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &UUIDInputLayout
    );

    VertexShaderCSO->Release();
    PixelShaderCSO->Release();
    ComputeShaderCSO->Release();
    ErrorBlob->Release();

    return hr;
}

void FRenderer::ReleaseUAV()
{
    if (UUIDMapTexture)
    {
        UUIDMapTexture->Release();
        UUIDMapTexture = nullptr;
    }
    if (UUIDTextureSRV)
    {
        UUIDTextureSRV->Release();
        UUIDTextureSRV = nullptr;
    }
    if (UUIDTextureUAV)
    {
        UUIDTextureUAV->Release();
        UUIDTextureUAV = nullptr;
    }
    if (UUIDListBuffer)
    {
        UUIDListBuffer->Release();
        UUIDListBuffer = nullptr;
    }
    if (UUIDListUAV)
    {
        UUIDListUAV->Release();
        UUIDListUAV = nullptr;
    }
}

void FRenderer::DiscardByUUID(const TArray<UPrimitiveComponent*>& InComponent, TArray<UPrimitiveComponent*>& OutComponent)
{
    //Graphics->DeviceContext->ClearDepthStencilView(Graphics->DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    PrepareRenderUUID(Graphics->DeviceContext);
    RenderUUID(InComponent, Graphics->DeviceContext);
    ReadValidUUID();
}

void FRenderer::PrepareRenderUUID(ID3D11DeviceContext* Context)
{
    Context->VSSetShader(UUIDVertexShader, nullptr, 0);
    Context->PSSetShader(UUIDPixelShader, nullptr, 0);
    Context->CSSetShader(UUIDComputeShader, nullptr, 0);
    Context->IASetInputLayout(UUIDInputLayout);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Context->RSSetViewports(1, &Graphics->Viewport);
     //Pixel Shader에 UAV 바인딩

    ID3D11UnorderedAccessView* uavs[] = { UUIDTextureUAV, UUIDListUAV };
    UINT initialCounts[] = { 0, 0 };
    Context->OMSetRenderTargetsAndUnorderedAccessViews(
        0, nullptr, Graphics->DepthStencilView, 1, 2, uavs, initialCounts);

    //Context->OMSetRenderTargetsAndUnorderedAccessViews(
        //0, nullptr, Graphics->DepthStencilView, 1, 1, &UUIDTextureUAV, nullptr);

    //Graphics->DeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(
    //    1, &Graphics->RTVs[0], Graphics->DepthStencilView, 1, 1, &UUIDTextureUAV, nullptr);
}

void FRenderer::RenderUUID(const TArray<UPrimitiveComponent*>& InComponent, ID3D11DeviceContext* Context)
{
    Graphics->DeviceContext->IASetInputLayout(UUIDInputLayout);
    uint32 Stride = sizeof(FVector);
    uint32 Offset = 0;

    Context->VSSetConstantBuffers(0, 1, &ConstantBuffer); // worldmatrix
    Context->PSSetConstantBuffers(0, 1, &ConstantBuffer); // uuid
    
    // Mesh별로 바꿔야함.
    for (UPrimitiveComponent* PrimComp : InComponent)
    {
        if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimComp))
        {
            OBJ::FStaticMeshRenderData* RenderData = MeshComp->GetStaticMesh()->GetRenderData();
            Context->IASetVertexBuffers(0, 1, &RenderData->VertexBufferPosOnly, &Stride, &Offset);
            if (RenderData->IndexBuffer)
                Context->IASetIndexBuffer(RenderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

            // 이제 viewport tick에서 해주고있음. RenderStaticMeshesThread도 수정해야함
            //Context->VSSetConstantBuffers(5, 1, &ConstantBufferView); // viewmatrix
            //Context->VSSetConstantBuffers(6, 1, &ConstantBufferProjection); // projmatrix

            if (ConstantBuffer)
            {
                D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR; // GPU�� �޸� �ּ� ����

                Context->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR); // update constant buffer every frame
                {
                    FConstants* constants = static_cast<FConstants*>(ConstantBufferMSR.pData);
                    constants->WorldMatrix = MeshComp->GetWorldMatrix();
                    constants->UUIDuint = MeshComp->GetUUID(); // float로 인코딩 안함. 바로 compute shader에 넣음
                }
                Context->Unmap(ConstantBuffer, 0);
            }
            Context->DrawIndexed(RenderData->Indices.Num(), 0, 0);
        }
    }

     //Compute Shader 실행 전에 UAV 해제
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    Graphics->DeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, &nullUAV, nullptr);
}

void FRenderer::ReadValidUUID()
{
    // UAV를 SRV로 전환

    // Compute Shader에서 사용할 리소스 바인딩
    Graphics->DeviceContext->CSSetShaderResources(0, 1, &UUIDTextureSRV);  // t0: UUIDTextureRead (읽기)
    Graphics->DeviceContext->CSSetUnorderedAccessViews(2, 1, &UUIDListUAV, nullptr);  // u2: UUIDList (쓰기)

    // Compute Shader 실행
    Graphics->DeviceContext->CSSetShader(UUIDComputeShader, nullptr, 0);
    Graphics->DeviceContext->Dispatch(Graphics->screenWidth / 16, Graphics->screenHeight / 16, 1);  // Thread groups 크기 설정

    // UAV에 대한 쓰기를 완료하려면 UAV의 상태를 마무리 처리합니다.
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    Graphics->DeviceContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);  // UAV를 해제


    // UUIDList의 내용을 읽기 위한 Map 작업
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = Graphics->DeviceContext->Map(UUIDListBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        uint32* pUUIDList = (uint32*)mappedResource.pData;

        // pUUIDList[0]에는 UUID 개수가 저장되어 있고, 그 이후에는 UUID 목록이 저장됩니다.
        uint32 uuidCount = pUUIDList[0];

        // UUID 리스트를 출력하거나 필요한 작업을 수행
        for (uint32 i = 1; i <= uuidCount; ++i)
        {
            uint32 uuid = pUUIDList[i];
            // 처리할 UUID 값을 사용
        }

        Graphics->DeviceContext->Unmap(UUIDListBuffer, 0);
    }


}


void FRenderer::RenderStaticMeshesThread(std::vector<FMeshData> DataArray, size_t i, size_t end, size_t tid,
    UMaterial* Material, const UStaticMesh* StaticMesh, 
    ID3D11CommandList* &CommandList)
{
    {
        ID3D11DeviceContext* Context = Graphics->DeferredContexts[tid];
        PrepareShaderDeferred(Context);
        OBJ::FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
        UINT offset = 0;
        // 버텍스 버퍼 업데이트
        Context->IASetVertexBuffers(0, 1, &RenderData->VertexBuffer, &Stride, &offset);
        if (RenderData->IndexBuffer)
        {
            Context->IASetIndexBuffer(RenderData->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        }

        Context->VSSetConstantBuffers(0, 1, &ConstantBuffer);
        Context->VSSetConstantBuffers(5, 1, &ConstantBufferView);
        Context->VSSetConstantBuffers(6, 1, &ConstantBufferProjection);

        Context->PSSetConstantBuffers(0, 1, &ConstantBuffer);
        Context->PSSetConstantBuffers(1, 1, &MaterialConstantBuffer);

        UpdateMaterialDeferred(Context, Material->GetMaterialInfo());
        Context->RSSetViewports(1, &Graphics->Viewport);
        Context->OMSetRenderTargets(1, &QuadRTV, Graphics->DepthStencilView);

        // Process each FMeshData in the chunk
        for (size_t j = i; j < end; ++j)
        {

            const FMeshData& Data = DataArray[j];
            FMatrix MVP = Data.WorldMatrix;
            UpdateConstantDeferred(Context, MVP, FVector4(), Data.bIsSelected);

            // Draw
            Context->DrawIndexed(Data.IndexCount, Data.IndexStart, 0);

        }
    }
}

void FRenderer::CreateQuad()
{
    // QuadShader
    ID3DBlob* VertexShaderCSO = nullptr;
    ID3DBlob* PixelShaderCSO = nullptr;
    ID3DBlob* Error;
    D3DCompileFromFile(L"Shaders/QuadShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &Error);
    Graphics->Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &QuadVertexShader);

    D3DCompileFromFile(L"Shaders/QuadShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, &Error);
    Graphics->Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &QuadPixelShader);

    D3D11_INPUT_ELEMENT_DESC QuadLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    Graphics->Device->CreateInputLayout(
        QuadLayout, ARRAYSIZE(QuadLayout), VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &QuadInputLayout
    );

    VertexShaderCSO->Release();
    PixelShaderCSO->Release();
    if (Error)
    {
        Error->Release();
    }

    // QuadVertexBuffer
    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.ByteWidth = sizeof(FQuadVertex) * 4;
    BufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    BufferDesc.CPUAccessFlags = 0;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = QuadVertices;
    
    Graphics->Device->CreateBuffer(&BufferDesc, &InitData, &QuadVertexBuffer);

    // QuadIndexBuffer
    BufferDesc.ByteWidth = sizeof(uint32) * 6;
    BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    InitData.pSysMem = QuadIndides;
    
    Graphics->Device->CreateBuffer(&BufferDesc, &InitData, &QuadIndexBuffer);

    // Render target
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = Graphics->screenWidth;
    TextureDesc.Height = Graphics->screenHeight;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.MiscFlags = 0;
    Graphics->Device->CreateTexture2D(&TextureDesc, nullptr, &QuadTexture);

    // SRV
    Graphics->Device->CreateShaderResourceView(QuadTexture, nullptr, &QuadTextureSRV);

    // RTV
    D3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDesc;
    ZeroMemory(&RenderTargetViewDesc, sizeof(RenderTargetViewDesc));
    RenderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    RenderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    Graphics->Device->CreateRenderTargetView(QuadTexture, &RenderTargetViewDesc, &QuadRTV);

    // Sampler
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MipLODBias = -1; 
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    Graphics->Device->CreateSamplerState(&SamplerDesc, &QuadSampler);
}

void FRenderer::ReleaseQuad()
{
    if (QuadInputLayout)
    {
        QuadInputLayout->Release();
        QuadInputLayout = nullptr;
    }
    if (QuadPixelShader)
    {
        QuadPixelShader->Release();
        QuadPixelShader = nullptr;
    }
    if (QuadVertexShader)
    {
        QuadVertexShader->Release();
        QuadVertexShader = nullptr;
    }
}

void FRenderer::PrepareQuad()
{
    Graphics->DeviceContext->OMSetRenderTargets(1, &Graphics->BackBufferRTV, nullptr);
    Graphics->DeviceContext->RSSetViewports(1, &Graphics->Viewport);
    
    Graphics->DeviceContext->VSSetShader(QuadVertexShader, nullptr, 0);
    Graphics->DeviceContext->PSSetShader(QuadPixelShader, nullptr, 0);

    uint32 Stride = sizeof(FQuadVertex);
    uint32 Offset = 0;
    Graphics->DeviceContext->IASetVertexBuffers(0, 1, &QuadVertexBuffer, &Stride, &Offset);
    Graphics->DeviceContext->IASetIndexBuffer(QuadIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    Graphics->DeviceContext->IASetInputLayout(QuadInputLayout);

    Graphics->DeviceContext->PSSetShaderResources(127, 1, &QuadTextureSRV);
}

void FRenderer::RenderQuad()
{
    Graphics->DeviceContext->DrawIndexed(6, 0, 0);
}

void FRenderer::PrepareResize()
{
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    if (QuadRTV)
    {
        QuadRTV->Release();
        QuadRTV = nullptr;
    }
    if (QuadTexture)
    {
        QuadTexture->Release();
        QuadTexture = nullptr;
    }
    if (QuadTextureSRV)
    {
        QuadTextureSRV->Release();
        QuadTextureSRV = nullptr;
    }
}

void FRenderer::OnResize(const DXGI_SWAP_CHAIN_DESC& SwapchainDesc)
{
    uint32 Width = Graphics->screenWidth;
    uint32 Height = Graphics->screenHeight;
    if (Width == 0 || Height == 0)
    {
        return;
    }

    // Create New
    HRESULT hr = S_OK;
    // Render target
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.MiscFlags = 0;
    hr = Graphics->Device->CreateTexture2D(&TextureDesc, nullptr, &QuadTexture);
    if (FAILED(hr))
    {
        return;
    }
    // SRV
    hr = Graphics->Device->CreateShaderResourceView(QuadTexture, nullptr, &QuadTextureSRV);
    if (FAILED(hr))
    {
        return;
    }
    // RTV
    D3D11_RENDER_TARGET_VIEW_DESC RenderTargetViewDesc;
    ZeroMemory(&RenderTargetViewDesc, sizeof(RenderTargetViewDesc));
    RenderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    RenderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = Graphics->Device->CreateRenderTargetView(QuadTexture, &RenderTargetViewDesc, &QuadRTV);
    if (FAILED(hr))
    {
        return;
    }

    

    D3D11_VIEWPORT Viewport = {
        .TopLeftX = 0.f,
        .TopLeftY = 0.f,
        .Width = static_cast<float>(Width),
        .Height = static_cast<float>(Height),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };
    Graphics->DeviceContext->RSSetViewports(1, &Viewport);
}

void FRenderer::RenderLight()
{
    for (auto Light : LightObjs)
    {
        FMatrix Model = JungleMath::CreateModelMatrix(Light->GetWorldLocation(), Light->GetWorldRotation(), {1, 1, 1});
        UPrimitiveBatch::GetInstance().AddCone(Light->GetWorldLocation(), Light->GetRadius(), 15, 140, Light->GetColor(), Model);
        UPrimitiveBatch::GetInstance().RenderOBB(Light->GetBoundingBox(), Light->GetWorldLocation(), Model);
    }
}
