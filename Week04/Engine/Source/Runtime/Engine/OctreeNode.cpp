#include "OctreeNode.h"

#include <filesystem>

#include "UnrealEd/EditorViewportClient.h"
#include "UObject/Casts.h"
#include "Engine/Classes/Components/PrimitiveComponent.h"
#include "Engine/Classes/Components/StaticMeshComponent.h"
#include <thread>

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

void FOctreeNode::Insert(UPrimitiveComponent* Component, int32 Depth)
{
    // 경계 상자 교차 확인
    if (!BoundBox.IntersectsAABB(Component->GetWorldBoundingBox()))
    {
        return;
    }
    
    // 리프 노드가 아닌 경우, 적절한 자식 노드에만 삽입
    if (!bIsLeaf)
    {
        for (int32 i = 0; i < 8; ++i)
        {
            if (Children[i]->BoundBox.IntersectsAABB(Component->GetWorldBoundingBox()))
            {
                Children[i]->Insert(Component, Depth + 1);
            }
        }
        return;
    }
    
    // 리프 노드에 컴포넌트 추가
    Components.Add(Component);
    
    // 분할 조건 확인
    if (Components.Num() > 32 && Depth < 4)
    {
        SubDivide();
        
        // 기존 컴포넌트를 적절한 자식 노드에만 재분배
        for (UPrimitiveComponent* Comp : Components)
        {
            for (int32 i = 0; i < 8; ++i)
            {
                if (Children[i]->BoundBox.IntersectsAABB(Comp->GetWorldBoundingBox()))
                {
                    Children[i]->Insert(Comp, Depth + 1);
                }
            }
        }
        
        // 리프 노드가 아니므로 컴포넌트 목록 비우기
        Components.Empty();
    }
}

void FOctreeNode::FrustumCull(const Frustum& Frustum, TArray<UPrimitiveComponent*>& OutComponents)
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

// 물체가 너무 적음.
void FOctreeNode::FrustumCullThreaded(const Frustum& Frustum, TArray<UPrimitiveComponent*>& OutComponents)
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
    TArray<std::thread> threads;
    TArray<UPrimitiveComponent*> OutComponentsThreaded[8];
    for (int32 i = 0; i < 8; ++i)
    {
        if (Children[i])
        {
            threads.Add(std::thread(&FOctreeNode::FrustumCull, Children[i].get(), std::ref(Frustum), std::ref(OutComponentsThreaded[i])));
        }
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    for (int32 i = 0; i < 8; ++i)
    {
        OutComponents += OutComponentsThreaded[i];
    }

}

bool FOctreeNode::RayIntersectsOctree(const FVector& PickPosition, const FVector& PickOrigin) const
{
    float tmin = -FLT_MAX;
    float tmax = FLT_MAX;
    FVector RayDir = PickPosition - PickOrigin;

    float invD;
    float t0;
    float t1;
    
    if (RayDir.x != 0.f)
    {
        invD = 1.0f / RayDir.x;
        t0 = (BoundBox.min.x - PickOrigin.x) * invD;
        t1 = (BoundBox.max.x - PickOrigin.x) * invD;
        if (invD < 0.0f)
        {
            std::swap(t0, t1);
        }
        tmin = FMath::Max(tmin, t0);
        tmax = FMath::Min(tmax, t1);
        if (tmax < tmin)
        {
            return false;
        }
    }
    
    if (RayDir.y != 0.f)
    {
        invD = 1.0f / RayDir.y;
        t0 = (BoundBox.min.y - PickOrigin.y) * invD;
        t1 = (BoundBox.max.y - PickOrigin.y) * invD;
        if (invD < 0.0f)
        {
            std::swap(t0, t1);
        }
        tmin = FMath::Max(tmin, t0);
        tmax = FMath::Min(tmax, t1);
        if (tmax < tmin)
        {
            return false;
        }
    }

    if (RayDir.z != 0.f)
    {
        invD = 1.0f / RayDir.z;
        t0 = (BoundBox.min.z - PickOrigin.z) * invD;
        t1 = (BoundBox.max.z - PickOrigin.z) * invD;
        if (invD < 0.0f)
        {
            std::swap(t0, t1);
        }
        tmin = FMath::Max(tmin, t0);
        tmax = FMath::Min(tmax, t1);
        if (tmax < tmin)
        {
            return false;
        }
    }
    
    return true;
}

void FOctreeNode::QueryByRay(const FVector& PickPosition, const FVector& PickOrigin, TArray<UPrimitiveComponent*>& OutComps)
{
    if (!RayIntersectsOctree(PickPosition, PickOrigin))
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
