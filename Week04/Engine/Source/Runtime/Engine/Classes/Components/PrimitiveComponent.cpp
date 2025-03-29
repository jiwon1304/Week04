#include "PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"

UPrimitiveComponent::UPrimitiveComponent()
{
    bIsInitialized = false;
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

void UPrimitiveComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

int UPrimitiveComponent::CheckRayIntersection(FVector& rayOrigin, FVector& rayDirection, float& pfNearHitDistance)
{
    //if (!AABB.Intersect(rayOrigin, rayDirection, pfNearHitDistance)) return 0;
    int nIntersections = 0;
    //if (staticMesh == nullptr) return 0;
    //FVertexSimple* vertices = staticMesh->vertices.get();
    //int vCount = staticMesh->numVertices;
    //UINT* indices = staticMesh->indices.get();
    //int iCount = staticMesh->numIndices;

    //if (!vertices) return 0;
    //BYTE* pbPositions = reinterpret_cast<BYTE*>(staticMesh->vertices.get());
    //
    //int nPrimitives = (!indices) ? (vCount / 3) : (iCount / 3);
    //float fNearHitDistance = FLT_MAX;
    //for (int i = 0; i < nPrimitives; i++) {
    //    int idx0, idx1, idx2;
    //    if (!indices) {
    //        idx0 = i * 3;
    //        idx1 = i * 3 + 1;
    //        idx2 = i * 3 + 2;
    //    }
    //    else {
    //        idx0 = indices[i * 3];
    //        idx2 = indices[i * 3 + 1];
    //        idx1 = indices[i * 3 + 2];
    //    }

    //    // 각 삼각형의 버텍스 위치를 FVector로 불러옵니다.
    //    uint32 stride = sizeof(FVertexSimple);
    //    FVector v0 = *reinterpret_cast<FVector*>(pbPositions + idx0 * stride);
    //    FVector v1 = *reinterpret_cast<FVector*>(pbPositions + idx1 * stride);
    //    FVector v2 = *reinterpret_cast<FVector*>(pbPositions + idx2 * stride);

    //    float fHitDistance;
    //    if (IntersectRayTriangle(rayOrigin, rayDirection, v0, v1, v2, fHitDistance)) {
    //        if (fHitDistance < fNearHitDistance) {
    //            pfNearHitDistance =  fNearHitDistance = fHitDistance;
    //        }
    //        nIntersections++;
    //    }
    //   
    //}
    return nIntersections;
}

bool UPrimitiveComponent::IntersectRayTriangle(const FVector& rayOrigin, const FVector& rayDirection, const FVector& v0, const FVector& v1, const FVector& v2, float& hitDistance)
{
    const float epsilon = 1e-6f;
    FVector edge1 = v1 - v0;
    const FVector edge2 = v2 - v0;
    FVector FrayDirection = rayDirection;
    FVector h = FrayDirection.Cross(edge2);
    float a = edge1.Dot(h);

    if (fabs(a) < epsilon)
        return false; // Ray와 삼각형이 평행한 경우

    float f = 1.0f / a;
    FVector s = rayOrigin - v0;
    float u = f * s.Dot(h);
    if (u < 0.0f || u > 1.0f)
        return false;

    FVector q = s.Cross(edge1);
    float v = f * FrayDirection.Dot(q);
    if (v < 0.0f || (u + v) > 1.0f)
        return false;

    float t = f * edge2.Dot(q);
    if (t > epsilon) {

        hitDistance = t;
        return true;
    }

    return false;
}

FBoundingBox UPrimitiveComponent::GetBoundingBox()
{
    if (!bIsInitialized) {
        FBoundingBox aabb = AABB;
        FVector pos = GetWorldLocation();
        FVector scale = GetWorldScale();
        FVector rotation = GetWorldRotation();
        if (scale != FVector(1.0f, 1.0f, 1.0f)) {
            FVector center = aabb.GetCenter();
            FVector extent = aabb.GetExtent();
            extent.x *= scale.x;
            extent.y *= scale.y;
            extent.z *= scale.z;
            aabb.min = center - extent;
            aabb.max = center + extent;
        }
        if (rotation != FVector(0.0f, 0.0f, 0.0f)) {
            FMatrix Rotation = FMatrix::CreateRotation(rotation.x, rotation.y, rotation.z);
            TArray<FVector> vertices = aabb.GetVertices();
            FVector min(FLT_MAX, FLT_MAX, FLT_MAX);
            FVector max = min * -1.0f;
            for (int i = 0; i < 8; ++i) {
                Rotation.TransformPosition(vertices[i]);
                FVector vertex = vertices[i];
                min.x = FMath::Min(min.x, vertex.x);
                min.y = FMath::Min(min.y, vertex.y);
                min.z = FMath::Min(min.z, vertex.z);
                max.x = FMath::Max(max.x, vertex.x);
                max.y = FMath::Max(max.y, vertex.y);
                max.z = FMath::Max(max.z, vertex.z);
            }
            aabb.min = min;
            aabb.max = max;
        }
        aabb.min = aabb.min + pos;
        aabb.max = aabb.max + pos;
        AABB = aabb;
        bIsInitialized = true;
    }
    return AABB;
}