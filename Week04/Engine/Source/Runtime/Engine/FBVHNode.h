#pragma once
#include "Define.h"

class FBVHNode
{
public:
    FBVHNode(FVector Min, FVector Max);
    
    void CreateVertexBVH(const TArray<FVertexSimple>& vertices, int depth, int maxDepth);
    bool RayIntersectsBVH(const FVector& RayOrigin, const FVector& RayDirection, float& OutHitDistance) const;
private:
    TArray<FBVHNode*> Children;
    TArray<FVertexSimple> Vertices;
    const FBoundingBox BoundBox; //현재 노드의 공간 범위
    bool bIsLeaf;
    void SubDivide();
};
