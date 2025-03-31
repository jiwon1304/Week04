#pragma once

#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "Define.h"

// --- Helper Structures ---

// 3D 벡터 (FVertexSimple의 포지션 계산용)
struct Vector3
{
    float X;
    float Y;
    float Z;
};

// 2D 벡터 (UV 계산용)
struct Vector2
{
    float U;
    float V;
};

// 4x4 행렬 (쿼드릭 저장용)
struct Matrix4
{
    float M[4][4];

    Matrix4()
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                M[i][j] = 0.0f;
            }
        }
    }

    Matrix4 operator+ (const Matrix4 &Other) const
    {
        Matrix4 Result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                Result.M[i][j] = M[i][j] + Other.M[i][j];
            }
        }
        return Result;
    }

    Matrix4& operator+= (const Matrix4 &Other)
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                M[i][j] += Other.M[i][j];
            }
        }
        return *this;
    }
};

// --- QEM 단순화용 내부 구조체 ---

// 단순화 시 사용할 정점 (FVertexSimple + 쿼드릭, 유효 여부)
struct FSimplifiedVertex
{
    FVertexSimple Vertex;
    Matrix4 Quadric;
    bool Valid;

    FSimplifiedVertex() : Valid(true) {}
};

// 삼각형 (세 정점 인덱스, 유효 여부)
struct FSimpleTriangle
{
    int Indices[3];
    bool Valid;

    FSimpleTriangle() : Valid(true)
    {
        Indices[0] = Indices[1] = Indices[2] = -1;
    }
};

// 엣지 정보 (두 정점 인덱스, 최적 정점 및 에러)
struct FEdge
{
    int V1;
    int V2;
    FVertexSimple OptimalVertex;
    float Error;
};

// --- Helper Functions ---

// 삼각형의 평면 방정식 (ax+by+cz+d=0)을 구함 (포지션만 사용)
void ComputePlane(const FVertexSimple &V0, const FVertexSimple &V1, const FVertexSimple &V2, float &A, float &B, float &C, float &D);

// 평면 방정식 계수로부터 쿼드릭 행렬 생성
Matrix4 ComputeQuadricForPlane(float A, float B, float C, float D);

// 두 FVertexSimple의 정보를 평균내어 (포지션, UV, Color) 최적 정점 생성
FVertexSimple ComputeOptimalVertex(const FVertexSimple &V1, const FVertexSimple &V2);

// 쿼드릭 행렬과 정점 정보를 사용해 포지션 에러 계산 (기존 QEM 방식)
float ComputeError(const Matrix4 &Quadric, const FVertexSimple &Vertex);

// 두 정점의 엣지 수축 에러 계산 (포지션 쿼드릭 에러 + UV 차이 패널티)
float ComputeEdgeError(const FSimplifiedVertex &Vertex1, const FSimplifiedVertex &Vertex2, const FVertexSimple &OptimalVertex, const Matrix4 &CombinedQuadric, float UVWeight);

// --- QEM 단순화 알고리즘 (서브메시별) ---
// Vertices: 단순화할 정점 배열 (내부적으로 FSimplifiedVertex로 관리)
// Triangles: 삼각형 배열 (각각 3개 인덱스)
// TargetVertexCount: 남길 목표 정점 수
// UVWeight: UV 차이에 곱할 가중치
void QEMSimplifySubmesh(std::vector<FSimplifiedVertex> &Vertices, std::vector<FSimpleTriangle> &Triangles, int TargetVertexCount, float UVWeight);

// --- 최종 함수: FStaticMeshRenderData에 QEM 단순화 적용 ---
// GlobalTargetVertexCount: 전체 FStaticMeshRenderData.Vertices.size()에 대한 목표 개수
// UVWeight: UV 에러에 곱할 가중치 (예: 1.0f)
void SimplifyStaticMeshRenderData(OBJ::FStaticMeshRenderData &MeshData, int GlobalTargetVertexCount, float UVWeight);
