#pragma once
#include "Components/MeshComponent.h"
#include "Mesh/StaticMesh.h"

class UStaticMeshComponent : public UMeshComponent
{
    DECLARE_CLASS(UStaticMeshComponent, UMeshComponent)

public:
    UStaticMeshComponent() = default;

    PROPERTY(int, selectedSubMeshIndex);

    virtual void SetLocation(FVector _newLoc) override;
    virtual void SetRotation(FVector _newRot) override;
    virtual void SetRotation(FQuat _newRot) override;
    virtual void SetScale(FVector _newScale) override;

    virtual uint32 GetNumMaterials() const override;
    virtual UMaterial* GetMaterial(uint32 ElementIndex) const override;
    virtual uint32 GetMaterialIndex(FName MaterialSlotName) const override;
    virtual TArray<FName> GetMaterialSlotNames() const override;
    virtual void GetUsedMaterials(TArray<UMaterial*>& Out) const override;

    virtual int CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance) override;
    
    UStaticMesh* GetStaticMesh() const { return staticMesh; }
    void SetStaticMesh(UStaticMesh* value)
    { 
        staticMesh = value;
        OverrideMaterials.SetNum(value->GetMaterials().Num());
        LocalAABB = FBoundingBox(staticMesh->GetRenderData()->BoundingBoxMin, staticMesh->GetRenderData()->BoundingBoxMax);
    }

    FMatrix GetWorldMatrix() const { return W04WorldMatrix; }
    void SetWorldMatrix(const FMatrix& value) { W04WorldMatrix = value; }

    bool bWasDrawnBefore = true;
    bool bAlreadyQueued = false;
protected:
    UStaticMesh* staticMesh = nullptr;
    int selectedSubMeshIndex = -1;

    FMatrix W04WorldMatrix;
};