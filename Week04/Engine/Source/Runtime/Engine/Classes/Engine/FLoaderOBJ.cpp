#include "FLoaderOBJ.h"
#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"
#include "Components/Mesh/StaticMesh.h"

UMaterial* FManagerOBJ::CreateMaterial(FObjMaterialInfo materialInfo)
{
    if (MaterialMap[materialInfo.MTLName] != nullptr)
        return MaterialMap[materialInfo.MTLName];

    UMaterial* newMaterial = FObjectFactory::ConstructObject<UMaterial>();
    newMaterial->SetMaterialInfo(materialInfo);
    MaterialMap.Add(materialInfo.MTLName, newMaterial);
    return newMaterial;
}

UMaterial* FManagerOBJ::GetMaterial(FString name)
{
    return MaterialMap[name];
}

UStaticMesh* FManagerOBJ::CreateStaticMesh(FString filePath)
{

    OBJ::FStaticMeshRenderData* StaticMeshRenderData = FManagerOBJ::LoadObjStaticMeshAsset(filePath);

    if (StaticMeshRenderData == nullptr) return nullptr;

    UStaticMesh* staticMesh = GetStaticMesh(StaticMeshRenderData->ObjectName);
    if (staticMesh != nullptr) {
        return staticMesh;
    }

    staticMesh = FObjectFactory::ConstructObject<UStaticMesh>();
    staticMesh->SetData(StaticMeshRenderData);

    StaticMeshMap.Add(StaticMeshRenderData->ObjectName, staticMesh);
}

UStaticMesh* FManagerOBJ::GetStaticMesh(FWString name)
{
    return StaticMeshMap[name];
}
