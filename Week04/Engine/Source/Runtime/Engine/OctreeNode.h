#pragma once
#include "Define.h"

class UPrimitiveComponent;
struct Frustum;

struct FOctreeNode
{
    FOctreeNode(FVector Min, FVector Max);

    void SubDivide();

    void Insert(UPrimitiveComponent* Component, int32 Depth = 0);

    void FrustumCull(const Frustum& Frustum, TArray<UPrimitiveComponent*>& OutComponents);

    void FrustumCullThreaded(const Frustum& Frustum, TArray<UPrimitiveComponent*>& OutComponents);

    bool RayIntersectsOctree(const FVector& PickPosition, const FVector& PickOrigin) const;

    void QueryByRay(const FVector& PickPosition, const FVector& PickOrigin, TArray<UPrimitiveComponent*>& OutComps);

    uint32 CountAllComponents() const;

    FBoundingBox BoundBox; //현재 노드의 공간 범위
    
    TArray<UPrimitiveComponent*> Components; //현재 노드에 포함된 물체 리스트
    
    TArray<std::unique_ptr<FOctreeNode>> Children; //자식 옥트리
    
    bool bIsLeaf;
};