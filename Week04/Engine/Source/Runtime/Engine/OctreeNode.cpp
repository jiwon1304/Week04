#include "OctreeNode.h"

#include "UnrealEd/EditorViewportClient.h"
#include "UObject/Casts.h"
#include "Engine/Classes/Components/PrimitiveComponent.h"
#include "Engine/Classes/Components/StaticMeshComponent.h"

FOctreeNode::FOctreeNode(FVector Min, FVector Max)
    : BoundBox(Min, Max)
    , Components(TArray<UPrimitiveComponent*>())
    , Children(TArray<std::unique_ptr<FOctreeNode>>(8))
    , bIsLeaf(true)
{}

void FOctreeNode::SubDivide()
{
    if (!bIsLeaf) return; // leaf node일 때만 쪼개기
    
    const FVector Center = BoundBox.GetCenter();
    const FVector Extent = BoundBox.GetExtent();
    const FVector Half = Extent * 0.5f;

    for (int32 i = 0; i < 8; ++i)
    {
        FVector Offset(
            (i & 1) ? Half.x : -Half.x,
            (i & 2) ? Half.y : -Half.y,
            (i & 4) ? Half.z : -Half.z
        );
        FVector ChildCenter = Center + Offset;
        FVector Min = ChildCenter - Half;
        FVector Max = ChildCenter + Half;

        Children[i] = std::make_unique<FOctreeNode>(Min, Max);
    }
    bIsLeaf = false;
}

bool FOctreeNode::Insert(UPrimitiveComponent* Component, int32 Depth)
{
    if (!BoundBox.IntersectsAABB(Component->GetWorldBoundingBox()))
    {
        return false;
    }
    
    if (!bIsLeaf)
    {
        for (int32 i = 0; i < 8; ++i)
        {
            if (Children[i]->Insert(Component, Depth + 1))
            {
                return true;
            }
        }
        Components.Add(Component);
        return true;
    }
    
    Components.Add(Component);
    if (Components.Num() > 8 && Depth < 24)
    {
        SubDivide();
        TArray<UPrimitiveComponent*> Temp = Components;
        Components.Empty();

        for (UPrimitiveComponent* Comp : Temp)
        {
            bool bInserted = false;
            for (int32 i = 0; i < 8; ++i)
            {
                if (Children[i]->Insert(Comp, Depth + 1))
                {
                    bInserted = true;
                    break;
                }
            }
            if (!bInserted)
            {
                Components.Add(Comp);
            }
        }
    }
    return true;
}

void FOctreeNode::FrustumCull(Frustum& Frustum, TArray<UPrimitiveComponent*>& OutComponents)
{
    if (!Frustum.Intersects(BoundBox))
    {
        return;
    }
    
    if (bIsLeaf)
    {
        for (UPrimitiveComponent* Comp : Components)
        {
            if (Frustum.Intersects(Comp->GetWorldBoundingBox()))
            {
                OutComponents.Add(Comp);
            }
        }
        return;
    }
    
    for (int32 i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            Children[i]->FrustumCull(Frustum, OutComponents);
        }
    }
}

bool FOctreeNode::RayIntersectsOctree(const FVector& PickPosition, const FVector& PickOrigin, float& tmin, float& tmax)
{
    tmin = -FLT_MAX;
    tmax = FLT_MAX;
    FVector RayDir = PickPosition - PickOrigin;
    float invD = 1.0f / RayDir.x;
    float t0 = (BoundBox.min.x - PickOrigin.x) * invD;
    float t1 = (BoundBox.max.x - PickOrigin.x) * invD;
    if (invD < 0.0f) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }
    tmin = FMath::Max(tmin, t0);
    tmax = FMath::Min(tmax, t1);
    if (tmax < tmin) {
        return false;
    }
    invD = 1.0f / RayDir.y;
    t0 = (BoundBox.min.y - PickOrigin.y) * invD;
    t1 = (BoundBox.max.y - PickOrigin.y) * invD;
    if (invD < 0.0f) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }
    tmin = FMath::Max(tmin, t0);
    tmax = FMath::Min(tmax, t1);
    if (tmax < tmin) {
        return false;
    }
    invD = 1.0f / RayDir.z;
    t0 = (BoundBox.min.z - PickOrigin.z) * invD;
    t1 = (BoundBox.max.z - PickOrigin.z) * invD;
    if (invD < 0.0f) {
        float temp = t0;
        t0 = t1;
        t1 = temp;
    }
    tmin = FMath::Max(tmin, t0);
    tmax = FMath::Min(tmax, t1);
    if (tmax < tmin) {
        return false;
    }
    return true;
}

void FOctreeNode::QueryByRay(const FVector& PickPosition, const FVector& PickOrigin, TArray<UPrimitiveComponent*>& OutComps)
{
    float tmin, tmax;
    if (!RayIntersectsOctree(PickPosition, PickOrigin, tmin, tmax))
        return;
    if (bIsLeaf)
    {
        for (UPrimitiveComponent* Comp : Components)
        {
            OutComps.Add(Comp);
        }
    }
    else
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
            {
                Children[i]->QueryByRay(PickPosition, PickOrigin, OutComps);
            }
        }
    }
}

uint32 FOctreeNode::CountAllComponents() const
{
    return Components.Num();
}
