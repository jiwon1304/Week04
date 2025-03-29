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
    if (!BoundBox.IntersectsAABB(Component->GetBoundingBox()))
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
    if (Components.Num() > 32 && Depth < 24)
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
            if (Frustum.Intersects(Comp->GetBoundingBox()))
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

uint32 FOctreeNode::CountAllComponents() const
{
    uint32 Count = Components.Num();
    if (!bIsLeaf)
    {
        for (int32 i = 0; i < 8; ++i)
        {
            if (Children[i])
            {
                Count += Children[i]->CountAllComponents();
            }
        }
    }
    return Count;
}
