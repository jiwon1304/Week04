#include "QEM.h"

#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>

// --- Helper Functions ---

void ComputePlane(const FVertexSimple &V0, const FVertexSimple &V1, const FVertexSimple &V2, float &A, float &B, float &C, float &D)
{
    Vector3 Edge1;
    Edge1.X = V1.x - V0.x;
    Edge1.Y = V1.y - V0.y;
    Edge1.Z = V1.z - V0.z;

    Vector3 Edge2;
    Edge2.X = V2.x - V0.x;
    Edge2.Y = V2.y - V0.y;
    Edge2.Z = V2.z - V0.z;

    Vector3 Normal;
    Normal.X = Edge1.Y * Edge2.Z - Edge1.Z * Edge2.Y;
    Normal.Y = Edge1.Z * Edge2.X - Edge1.X * Edge2.Z;
    Normal.Z = Edge1.X * Edge2.Y - Edge1.Y * Edge2.X;

    float Len = std::sqrt(Normal.X * Normal.X + Normal.Y * Normal.Y + Normal.Z * Normal.Z);
    if (Len != 0.0f)
    {
        Normal.X /= Len;
        Normal.Y /= Len;
        Normal.Z /= Len;
    }
    A = Normal.X;
    B = Normal.Y;
    C = Normal.Z;
    D = -(A * V0.x + B * V0.y + C * V0.z);
}

Matrix4 ComputeQuadricForPlane(float A, float B, float C, float D)
{
    Matrix4 Q;
    float Vec[4] = { A, B, C, D };
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            Q.M[i][j] = Vec[i] * Vec[j];
        }
    }
    return Q;
}

FVertexSimple ComputeOptimalVertex(const FVertexSimple &V1, const FVertexSimple &V2)
{
    FVertexSimple Result;
    Result.x = (V1.x + V2.x) * 0.5f;
    Result.y = (V1.y + V2.y) * 0.5f;
    Result.z = (V1.z + V2.z) * 0.5f;
    Result.u = (V1.u + V2.u) * 0.5f;
    Result.v = (V1.v + V2.v) * 0.5f;
    Result.r = (V1.r + V2.r) * 0.5f;
    Result.g = (V1.g + V2.g) * 0.5f;
    Result.b = (V1.b + V2.b) * 0.5f;
    Result.a = (V1.a + V2.a) * 0.5f;
    return Result;
}

float ComputeError(const Matrix4 &Quadric, const FVertexSimple &Vertex)
{
    float Vec[4] = { Vertex.x, Vertex.y, Vertex.z, 1.0f };
    float Error = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            Error += Vec[i] * Quadric.M[i][j] * Vec[j];
        }
    }
    return Error;
}

// UV seam 보존을 위해 두 정점의 UV 차이가 임계값 이상이면 병합을 피하도록 함
float ComputeEdgeError(const FSimplifiedVertex &Vertex1, const FSimplifiedVertex &Vertex2, const FVertexSimple &OptimalVertex, const Matrix4 &CombinedQuadric, float UVWeight)
{
    const float UVSeamThreshold = 0.1f;
    if (std::fabs(Vertex1.Vertex.u - Vertex2.Vertex.u) > UVSeamThreshold ||
        std::fabs(Vertex1.Vertex.v - Vertex2.Vertex.v) > UVSeamThreshold)
    {
        return std::numeric_limits<float>::max();
    }

    float PositionError = ComputeError(CombinedQuadric, OptimalVertex);
    float UVDiffU = Vertex1.Vertex.u - Vertex2.Vertex.u;
    float UVDiffV = Vertex1.Vertex.v - Vertex2.Vertex.v;
    float UVError = UVDiffU * UVDiffU + UVDiffV * UVDiffV;
    return PositionError + UVWeight * UVError;
}

// --- QEM 단순화 알고리즘 (서브메시별) ---

void QEMSimplifySubmesh(std::vector<FSimplifiedVertex> &Vertices, std::vector<FSimpleTriangle> &Triangles, uint32 TargetVertexCount, float UVWeight)
{
    // 1. 각 삼각형의 평면으로 정점 쿼드릭 누적
    for (size_t t = 0; t < Triangles.size(); t++)
    {
        if (!Triangles[t].Valid)
        {
            continue;
        }
        int Idx0 = Triangles[t].Indices[0];
        int Idx1 = Triangles[t].Indices[1];
        int Idx2 = Triangles[t].Indices[2];

        float A, B, C, D;
        ComputePlane(Vertices[Idx0].Vertex, Vertices[Idx1].Vertex, Vertices[Idx2].Vertex, A, B, C, D);
        Matrix4 Q = ComputeQuadricForPlane(A, B, C, D);
        Vertices[Idx0].Quadric += Q;
        Vertices[Idx1].Quadric += Q;
        Vertices[Idx2].Quadric += Q;
    }

    // 2. 엣지 리스트 구성 (중복 제거)
    std::set<std::pair<int, int>> EdgeSet;
    std::vector<FEdge> Edges;
    for (size_t t = 0; t < Triangles.size(); t++)
    {
        if (!Triangles[t].Valid)
        {
            continue;
        }
        for (int i = 0; i < 3; i++)
        {
            int IdxA = Triangles[t].Indices[i];
            int IdxB = Triangles[t].Indices[(i + 1) % 3];
            if (IdxA > IdxB)
            {
                std::swap(IdxA, IdxB);
            }
            if (EdgeSet.find({ IdxA, IdxB }) == EdgeSet.end())
            {
                EdgeSet.insert({ IdxA, IdxB });
                FEdge Edge;
                Edge.V1 = IdxA;
                Edge.V2 = IdxB;
                Edge.OptimalVertex = ComputeOptimalVertex(Vertices[IdxA].Vertex, Vertices[IdxB].Vertex);
                Matrix4 CombinedQuadric = Vertices[IdxA].Quadric + Vertices[IdxB].Quadric;
                Edge.Error = ComputeEdgeError(Vertices[IdxA], Vertices[IdxB], Edge.OptimalVertex, CombinedQuadric, UVWeight);
                Edges.push_back(Edge);
            }
        }
    }

    int CurrentValidCount = 0;
    for (size_t i = 0; i < Vertices.size(); i++)
    {
        if (Vertices[i].Valid)
        {
            CurrentValidCount++;
        }
    }
    while (CurrentValidCount > TargetVertexCount && !Edges.empty())
    {
        auto MinEdgeIt = std::min_element(Edges.begin(), Edges.end(), [](const FEdge &E1, const FEdge &E2)
        {
            return E1.Error < E2.Error;
        });
        FEdge MinEdge = *MinEdgeIt;
        Edges.erase(MinEdgeIt);

        int VKeep = MinEdge.V1;
        int VRemove = MinEdge.V2;
        if (!Vertices[VKeep].Valid || !Vertices[VRemove].Valid)
        {
            continue;
        }
        Vertices[VKeep].Vertex = MinEdge.OptimalVertex;
        Vertices[VKeep].Quadric = Vertices[VKeep].Quadric + Vertices[VRemove].Quadric;
        Vertices[VRemove].Valid = false;
        CurrentValidCount--;

        for (size_t t = 0; t < Triangles.size(); t++)
        {
            if (!Triangles[t].Valid)
            {
                continue;
            }
            for (int i = 0; i < 3; i++)
            {
                if (Triangles[t].Indices[i] == VRemove)
                {
                    Triangles[t].Indices[i] = VKeep;
                }
            }
            if (Triangles[t].Indices[0] == Triangles[t].Indices[1] ||
                Triangles[t].Indices[1] == Triangles[t].Indices[2] ||
                Triangles[t].Indices[2] == Triangles[t].Indices[0])
            {
                Triangles[t].Valid = false;
            }
        }

        Edges.clear();
        EdgeSet.clear();
        for (size_t t = 0; t < Triangles.size(); t++)
        {
            if (!Triangles[t].Valid)
            {
                continue;
            }
            for (int i = 0; i < 3; i++)
            {
                int IdxA = Triangles[t].Indices[i];
                int IdxB = Triangles[t].Indices[(i + 1) % 3];
                if (IdxA > IdxB)
                {
                    std::swap(IdxA, IdxB);
                }
                if (EdgeSet.find({ IdxA, IdxB }) == EdgeSet.end())
                {
                    EdgeSet.insert({ IdxA, IdxB });
                    if (!Vertices[IdxA].Valid || !Vertices[IdxB].Valid)
                    {
                        continue;
                    }
                    FEdge Edge;
                    Edge.V1 = IdxA;
                    Edge.V2 = IdxB;
                    Edge.OptimalVertex = ComputeOptimalVertex(Vertices[IdxA].Vertex, Vertices[IdxB].Vertex);
                    Matrix4 CombinedQuadric = Vertices[IdxA].Quadric + Vertices[IdxB].Quadric;
                    Edge.Error = ComputeEdgeError(Vertices[IdxA], Vertices[IdxB], Edge.OptimalVertex, CombinedQuadric, UVWeight);
                    Edges.push_back(Edge);
                }
            }
        }
    }

    std::vector<FSimplifiedVertex> FinalVertices;
    std::vector<int> VertexMap(Vertices.size(), -1);
    for (size_t i = 0; i < Vertices.size(); i++)
    {
        if (Vertices[i].Valid)
        {
            VertexMap[i] = static_cast<int>(FinalVertices.size());
            FinalVertices.push_back(Vertices[i]);
        }
    }
    for (size_t t = 0; t < Triangles.size(); t++)
    {
        if (!Triangles[t].Valid)
        {
            continue;
        }
        for (int i = 0; i < 3; i++)
        {
            Triangles[t].Indices[i] = VertexMap[Triangles[t].Indices[i]];
        }
    }
    Vertices = FinalVertices;
}

// --- 최종 함수: FStaticMeshRenderData에 QEM 단순화 적용 ---

void SimplifyStaticMeshRenderData(OBJ::FStaticMeshRenderData &MeshData, uint32 GlobalTargetVertexCount, float UVWeight)
{
    std::vector<FVertexSimple> FinalVertices;
    std::vector<unsigned int> FinalIndices;
    std::vector<FMaterialSubset> FinalMaterialSubsets;

    for (size_t m = 0; m < MeshData.MaterialSubsets.Num(); m++)
    {
        FMaterialSubset &Subset = MeshData.MaterialSubsets[m];

        int IndexStart = Subset.IndexStart;
        int IndexCount = Subset.IndexCount;
        std::map<unsigned int, int> GlobalToLocalMap;
        std::vector<FSimplifiedVertex> SubVertices;
        std::vector<FSimpleTriangle> SubTriangles;

        for (int i = 0; i < IndexCount; i += 3)
        {
            FSimpleTriangle Triangle;
            for (int j = 0; j < 3; j++)
            {
                unsigned int GlobalIdx = MeshData.Indices[IndexStart + i + j];
                auto It = GlobalToLocalMap.find(GlobalIdx);
                int LocalIdx = 0;
                if (It == GlobalToLocalMap.end())
                {
                    LocalIdx = static_cast<int>(SubVertices.size());
                    GlobalToLocalMap[GlobalIdx] = LocalIdx;
                    FSimplifiedVertex SimpleVertex;
                    SimpleVertex.Vertex = MeshData.Vertices[GlobalIdx];
                    SimpleVertex.Valid = true;
                    SubVertices.push_back(SimpleVertex);
                }
                else
                {
                    LocalIdx = It->second;
                }
                Triangle.Indices[j] = LocalIdx;
            }
            SubTriangles.push_back(Triangle);
        }

        int SubVertexCount = static_cast<int>(SubVertices.size());
        int LocalTarget = std::max(3, (int)(SubVertexCount * (GlobalTargetVertexCount / (float)MeshData.Vertices.Num())));
        QEMSimplifySubmesh(SubVertices, SubTriangles, LocalTarget, UVWeight);

        int GlobalVertexOffset = static_cast<int>(FinalVertices.size());
        for (size_t i = 0; i < SubVertices.size(); i++)
        {
            FinalVertices.push_back(SubVertices[i].Vertex);
        }
        int SubIndexCount = 0;
        for (size_t t = 0; t < SubTriangles.size(); t++)
        {
            if (!SubTriangles[t].Valid)
            {
                continue;
            }
            for (int j = 0; j < 3; j++)
            {
                FinalIndices.push_back(static_cast<unsigned int>(GlobalVertexOffset + SubTriangles[t].Indices[j]));
                SubIndexCount++;
            }
        }

        FMaterialSubset NewSubset;
        NewSubset.IndexStart = static_cast<unsigned int>(FinalIndices.size() - SubIndexCount);
        NewSubset.IndexCount = static_cast<unsigned int>(SubIndexCount);
        NewSubset.MaterialIndex = Subset.MaterialIndex;
        NewSubset.MaterialName = Subset.MaterialName;
        FinalMaterialSubsets.push_back(NewSubset);
    }

    MeshData.Vertices = FinalVertices;
    MeshData.Indices = FinalIndices;
    MeshData.MaterialSubsets = FinalMaterialSubsets;
}
