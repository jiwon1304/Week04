#include "FBVHNode.h"

FBVHNode::FBVHNode(FVector Min, FVector Max)
    : BoundBox(Min, Max)
    , Vertices(TArray<FVertexSimple>())
    , Children(TArray<FBVHNode*>(8))
    , bIsLeaf(true)
    {}

// 정점 기반 BVH 생성도 개선
void FBVHNode::CreateVertexBVH(const TArray<FVertexSimple>& vertices, int depth, int maxDepth)
{
    // 적은 수의 정점에서는 더 분할할 필요가 없음
    if (depth > maxDepth || vertices.Num() <= 5)
    {
        // 리프 노드에 정점 저장
        Vertices = vertices;
        bIsLeaf = true;
        return;
    }

    // 공간 분할
    SubDivide();
    
    // 각 자식 노드에 대한 정점 분류
    TArray<TArray<FVertexSimple>> childVertices;
    childVertices.SetNum(8);
    
    for (const auto& vertex : vertices)
    {
        FVector point(vertex.x, vertex.y, vertex.z);
        
        // 정점이 어떤 자식 노드에 속하는지 확인
        for (int i = 0; i < 8; i++)
        {
            if (Children[i]->BoundBox.ContainsPoint(point))
            {
                childVertices[i].Add(vertex);
                break; // 한 정점은 하나의 자식 노드에만 속함
            }
        }
    }
    
    // 각 자식 노드에 대해 재귀적으로 BVH 생성
    for (int i = 0; i < 8; i++)
    {
        if (childVertices[i].Num() == 0)
        {
            Children[i] = nullptr;
            continue;
        }
        
        Children[i]->CreateVertexBVH(childVertices[i], depth + 1, maxDepth);
    }
}

bool FBVHNode::RayIntersectsBVH(const FVector& RayOrigin, const FVector& RayDirection, float& OutHitDistance) const
{
    // 현재 노드의 바운딩 박스와 레이의 교차 여부 검사
    float tMin = FLT_MAX;
    if (!BoundBox.Intersect(RayOrigin, RayDirection, tMin))
    {
        return false; // 바운딩 박스와 교차하지 않으면 제외
    }
    
    // 리프 노드인 경우 정점 기반 검사 수행
    if (Children.Num() == 0 || bIsLeaf)
    {
        bool hitFound = false;
        float closestDistance = FLT_MAX;
        
        // 정점과의 거리 계산
        for (const auto& Vertex : Vertices)
        {
            FVector point(Vertex.x, Vertex.y, Vertex.z);
            float distance = point.DistanceFromPointToRay(RayOrigin, RayDirection);
            
            const float VertexThreshold = 0.1f;
            if (distance < VertexThreshold)
            {
                // 레이 방향으로의 거리 계산
                FVector toVertex = point - RayOrigin;
                float rayDistance = toVertex.Dot(RayDirection);
                
                if (rayDistance > 0.0f && rayDistance < closestDistance)
                {
                    closestDistance = rayDistance;
                    hitFound = true;
                }
            }
        }
        
        if (hitFound)
        {
            OutHitDistance = closestDistance;
            return true;
        }
        
        return false;
    }
    
    // 자식 노드 순회 (원래 코드와 동일하게 유지)
    bool hit = false;
    float closestDistance = FLT_MAX;
    
    // 거리에 따라 자식 노드 정렬
    struct ChildDistance { int ChildIndex; float Distance; };
    TArray<ChildDistance> SortedChildren;
    
    for (int i = 0; i < Children.Num(); ++i)
    {
        if (Children[i] != nullptr)
        {
            float distance;
            if (Children[i]->BoundBox.Intersect(RayOrigin, RayDirection, distance))
            {
                SortedChildren.Add({i, distance});
            }
        }
    }
    
    // 가까운 노드부터 순회
    SortedChildren.Sort([](const ChildDistance& A, const ChildDistance& B) {
        return A.Distance < B.Distance;
    });
    
    for (const auto& SortedChild : SortedChildren)
    {
        float childHitDistance = FLT_MAX;
        if (Children[SortedChild.ChildIndex]->RayIntersectsBVH(RayOrigin, RayDirection, childHitDistance))
        {
            if (childHitDistance < closestDistance)
            {
                closestDistance = childHitDistance;
                hit = true;
            }
        }
    }
    
    if (hit)
    {
        OutHitDistance = closestDistance;
        return true;
    }
    
    return false;
}

void FBVHNode::SubDivide()
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

        Children[i] = new FBVHNode(Min, Max);
    }
    bIsLeaf = false;
}