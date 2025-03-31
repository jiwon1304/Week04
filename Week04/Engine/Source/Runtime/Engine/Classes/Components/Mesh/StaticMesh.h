#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh();
    virtual ~UStaticMesh() override;
    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;
    OBJ::FStaticMeshRenderData* GetRenderData(uint8 LOD = 0) const;

    void SetData(OBJ::FStaticMeshRenderData* renderData);

private:
    TArray<FStaticMaterial*> materials;

    TArray<OBJ::FStaticMeshRenderData*> LODRenderData;

    void PrepareBuffers(OBJ::FStaticMeshRenderData*& Data);

    OBJ::FStaticMeshRenderData* DuplicateRenderData(const OBJ::FStaticMeshRenderData* InData)
    {
        OBJ::FStaticMeshRenderData* NewData = new OBJ::FStaticMeshRenderData;
        NewData->ObjectName = InData->ObjectName;
        NewData->PathName = InData->PathName;
        NewData->DisplayName = InData->DisplayName;
        
        NewData->Vertices = InData->Vertices;
        NewData->Indices = InData->Indices;

        NewData->Materials = InData->Materials;
        NewData->MaterialSubsets = InData->MaterialSubsets;
        
        NewData->BoundingBoxMin = InData->BoundingBoxMin;
        NewData->BoundingBoxMax = InData->BoundingBoxMax;
        
        NewData->VertexBuffer = nullptr;
        NewData->IndexBuffer = nullptr;
        
        return NewData;
    }
};
