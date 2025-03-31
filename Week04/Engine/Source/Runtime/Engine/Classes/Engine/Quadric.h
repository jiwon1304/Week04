#pragma once

#include <cmath>
#include <DirectXMath.h>

using namespace DirectX;

class FQuadric {
public:
    // 4x4 대칭행렬 저장 (행렬 전체 저장)
    float m[4][4];

    // 기본 생성자: 0으로 초기화
    FQuadric()
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                m[i][j] = 0.0f;
            }
        }
    }

    // 평면 계수 (a, b, c, d)를 이용한 생성자.
    // 평면: ax + by + cz + d = 0 일 때, Q = [a^2, ab, ac, ad; ab, b^2, bc, bd; ac, bc, c^2, cd; ad, bd, cd, d^2]
    FQuadric(float a, float b, float c, float d)
    {
        m[0][0] = a * a; m[0][1] = a * b; m[0][2] = a * c; m[0][3] = a * d;
        m[1][0] = a * b; m[1][1] = b * b; m[1][2] = b * c; m[1][3] = b * d;
        m[2][0] = a * c; m[2][1] = b * c; m[2][2] = c * c; m[2][3] = c * d;
        m[3][0] = a * d; m[3][1] = b * d; m[3][2] = c * d; m[3][3] = d * d;
    }

    // 덧셈 연산자: 요소별 덧셈
    FQuadric operator+(const FQuadric& other) const
    {
        FQuadric result;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.m[i][j] = m[i][j] + other.m[i][j];
            }
        }
        return result;
    }

    // 덧셈 대입 연산자
    FQuadric& operator+=(const FQuadric& other)
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                m[i][j] += other.m[i][j];
            }
        }
        return *this;
    }

    // 주어진 3D 점 v (FVector)와 항상 1인 동차좌표를 포함한 4D 벡터에 대해 비용 계산: v^T * Q * v
    float Evaluate(const FVector& v) const
    {
        float vec[4] = { v.x, v.y, v.z, 1.0f };
        float sum = 0.0f;
        for (int i = 0; i < 4; i++)
        {
            float rowSum = 0.0f;
            for (int j = 0; j < 4; j++)
            {
                rowSum += m[i][j] * vec[j];
            }
            sum += vec[i] * rowSum;
        }
        return sum;
    }

    // ComputeOptimalPosition: QEM 최적 위치 계산.
    // 3x3 행렬 A (좌상단)와 3x1 벡터 b (-m[0..2][3])를 구하여 A * v = -b 형태로 풀어 v를 계산.
    // 행렬 A의 역행렬 존재 시 optimal 위치를 반환하며, 그렇지 않으면 false 반환.
    bool ComputeOptimalPosition(FVector& optimal) const
    {
        // A = 3x3 행렬, b = 3x1 벡터
        float A[3][3] = {
            { m[0][0], m[0][1], m[0][2] },
            { m[1][0], m[1][1], m[1][2] },
            { m[2][0], m[2][1], m[2][2] }
        };
        float b[3] = { -m[0][3], -m[1][3], -m[2][3] };

        // 행렬식 계산
        float det = A[0][0]*(A[1][1]*A[2][2]-A[1][2]*A[2][1])
                  - A[0][1]*(A[1][0]*A[2][2]-A[1][2]*A[2][0])
                  + A[0][2]*(A[1][0]*A[2][1]-A[1][1]*A[2][0]);

        if (std::fabs(det) < 1e-6f)
        {
            return false; // 역행렬이 존재하지 않음
        }

        // 역행렬 A_inv 계산 (Cramer's rule 이용)
        float invDet = 1.0f / det;
        float A_inv[3][3];
        A_inv[0][0] =  (A[1][1]*A[2][2]-A[1][2]*A[2][1]) * invDet;
        A_inv[0][1] = -(A[0][1]*A[2][2]-A[0][2]*A[2][1]) * invDet;
        A_inv[0][2] =  (A[0][1]*A[1][2]-A[0][2]*A[1][1]) * invDet;

        A_inv[1][0] = -(A[1][0]*A[2][2]-A[1][2]*A[2][0]) * invDet;
        A_inv[1][1] =  (A[0][0]*A[2][2]-A[0][2]*A[2][0]) * invDet;
        A_inv[1][2] = -(A[0][0]*A[1][2]-A[0][2]*A[1][0]) * invDet;

        A_inv[2][0] =  (A[1][0]*A[2][1]-A[1][1]*A[2][0]) * invDet;
        A_inv[2][1] = -(A[0][0]*A[2][1]-A[0][1]*A[2][0]) * invDet;
        A_inv[2][2] =  (A[0][0]*A[1][1]-A[0][1]*A[1][0]) * invDet;

        // 최적 점 v = A_inv * b
        optimal.x = A_inv[0][0]*b[0] + A_inv[0][1]*b[1] + A_inv[0][2]*b[2];
        optimal.y = A_inv[1][0]*b[0] + A_inv[1][1]*b[1] + A_inv[1][2]*b[2];
        optimal.z = A_inv[2][0]*b[0] + A_inv[2][1]*b[1] + A_inv[2][2]*b[2];

        return true;
    }
};