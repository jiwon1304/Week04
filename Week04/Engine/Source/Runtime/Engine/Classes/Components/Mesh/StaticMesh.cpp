#include "StaticMesh.h"
#include "Engine/FLoaderOBJ.h"
#include "UObject/ObjectFactory.h"

#include "Engine/QEM.h"

UStaticMesh::UStaticMesh()
{

}

UStaticMesh::~UStaticMesh()
{
    for (auto& renderData : LODRenderData)
    {
        if (renderData == nullptr)
        {
            continue;
        }

        if (renderData->VertexBuffer)
        {
            renderData->VertexBuffer->Release();
            renderData->VertexBuffer = nullptr;
        }

        if (renderData->IndexBuffer)
        {
            renderData->IndexBuffer->Release();
            renderData->IndexBuffer = nullptr;
        }
    }
}

uint32 UStaticMesh::GetMaterialIndex(FName MaterialSlotName) const
{
    for (uint32 materialIndex = 0; materialIndex < materials.Num(); materialIndex++)
    {
        if (materials[materialIndex]->MaterialSlotName == MaterialSlotName)
        {
            return materialIndex;
        }
    }

    return -1;
}

void UStaticMesh::GetUsedMaterials(TArray<UMaterial*>& Out) const
{
    for (const FStaticMaterial* Material : materials)
    {
        Out.Emplace(Material->Material);
    }
}

OBJ::FStaticMeshRenderData* UStaticMesh::GetRenderData(uint8 LOD) const
{
    LOD = FMath::Max(static_cast<uint8>(0), FMath::Min(LOD, static_cast<uint8>(4)));
    return LODRenderData[LOD];
}

void UStaticMesh::SetData(OBJ::FStaticMeshRenderData* renderData)
{
    OBJ::FStaticMeshRenderData* Data = renderData;
    LODRenderData.Add(Data);
    PrepareBuffers(Data);

    uint32 TargerVertexNum = renderData->Vertices.Num();

    OBJ::FStaticMeshRenderData* LOD1 = DuplicateRenderData(renderData);
    SimplifyStaticMeshRenderData(*LOD1, TargerVertexNum * 0.8f, 10.f);
    LODRenderData.Add(LOD1);
    PrepareBuffers(LOD1);

    OBJ::FStaticMeshRenderData* LOD2 = DuplicateRenderData(renderData);
    SimplifyStaticMeshRenderData(*LOD2, TargerVertexNum * 0.5f, 10.f);
    LODRenderData.Add(LOD2);
    PrepareBuffers(LOD2);

    OBJ::FStaticMeshRenderData* LOD3 = DuplicateRenderData(renderData);
    SimplifyStaticMeshRenderData(*LOD3, TargerVertexNum * 0.3f, 10.f);
    LODRenderData.Add(LOD3);
    PrepareBuffers(LOD3);
    
    // Prepare Material
    for (int materialIndex = 0; materialIndex < renderData->Materials.Num(); materialIndex++) {
        FStaticMaterial* newMaterialSlot = new FStaticMaterial();
        UMaterial* newMaterial = FManagerOBJ::CreateMaterial(renderData->Materials[materialIndex]);

        newMaterialSlot->Material = newMaterial;
        newMaterialSlot->MaterialSlotName = renderData->Materials[materialIndex].MTLName;

        materials.Add(newMaterialSlot);
    }
}

void UStaticMesh::PrepareBuffers(OBJ::FStaticMeshRenderData*& Data)
{
    uint32 verticeNum = Data->Vertices.Num();
    if (verticeNum <= 0) return;
    Data->VertexBuffer = GetEngine().Renderer.CreateVertexBuffer(Data->Vertices, verticeNum * sizeof(FVertexSimple));

    uint32 indexNum = Data->Indices.Num();
    if (indexNum > 0)
        Data->IndexBuffer = GetEngine().Renderer.CreateIndexBuffer(Data->Indices, indexNum * sizeof(uint32));
}
