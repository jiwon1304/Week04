#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/Material/Material.h"
#include "Define.h"
#include "FBVHNode.h"

class UStaticMesh : public UObject
{
    DECLARE_CLASS(UStaticMesh, UObject)

public:
    UStaticMesh();
    virtual ~UStaticMesh() override;
    const TArray<FStaticMaterial*>& GetMaterials() const { return materials; }
    uint32 GetMaterialIndex(FName MaterialSlotName) const;
    void GetUsedMaterials(TArray<UMaterial*>& Out) const;
    OBJ::FStaticMeshRenderData* GetRenderData() const { return staticMeshRenderData; }

    bool CheckRayIntersect(const FVector& PickPosition, const FVector& rayOrigin, float& HitDistance) const;

    void SetData(OBJ::FStaticMeshRenderData* renderData);
private:
    OBJ::FStaticMeshRenderData* staticMeshRenderData = nullptr;
    TArray<FStaticMaterial*> materials;
    FBVHNode* MeshBVHNode;
};
