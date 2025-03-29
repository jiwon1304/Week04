#pragma once

#include <DirectXMath.h>

#include "MathUtility.h"
#include "Vector4.h"
#include "Vector.h"

using namespace DirectX;

// 4x4 행렬 연산
struct FMatrix
{
    float M[4][4];
    static const FMatrix Identity;

    FMatrix operator+(const FMatrix& Other) const
    {
        XMMATRIX xm1 = this->ToXMMATRIX();
        XMMATRIX xm2 = Other.ToXMMATRIX();
        XMMATRIX xmOut(
            XMVectorAdd(xm1.r[0], xm2.r[0]),
            XMVectorAdd(xm1.r[1], xm2.r[1]),
            XMVectorAdd(xm1.r[2], xm2.r[2]),
            XMVectorAdd(xm1.r[3], xm2.r[3])
        );
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator-(const FMatrix& Other) const
    {
        XMMATRIX xm1 = this->ToXMMATRIX();
        XMMATRIX xm2 = Other.ToXMMATRIX();
        XMMATRIX xmOut(
            XMVectorSubtract(xm1.r[0], xm2.r[0]),
            XMVectorSubtract(xm1.r[1], xm2.r[1]),
            XMVectorSubtract(xm1.r[2], xm2.r[2]),
            XMVectorSubtract(xm1.r[3], xm2.r[3])
        );
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator*(const FMatrix& Other) const
    {
        XMMATRIX xm1 = this->ToXMMATRIX();
        XMMATRIX xm2 = Other.ToXMMATRIX();
        XMMATRIX xmOut = XMMatrixMultiply(xm1, xm2);
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator*(float Scalar) const
    {
        XMMATRIX xm = this->ToXMMATRIX();
        XMVECTOR s = XMVectorReplicate(Scalar);
        XMMATRIX xmOut(
            XMVectorMultiply(xm.r[0], s),
            XMVectorMultiply(xm.r[1], s),
            XMVectorMultiply(xm.r[2], s),
            XMVectorMultiply(xm.r[3], s)
        );
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator/(float Scalar) const
    {
        float inv = 1.0f / Scalar;
        XMMATRIX xm = this->ToXMMATRIX();
        XMVECTOR invS = XMVectorReplicate(inv);
        XMMATRIX xmOut(
            XMVectorMultiply(xm.r[0], invS),
            XMVectorMultiply(xm.r[1], invS),
            XMVectorMultiply(xm.r[2], invS),
            XMVectorMultiply(xm.r[3], invS)
        );
        return FromXMMATRIX(xmOut);
    }

    float* operator[](int row) { return M[row]; }
    const float* operator[](int row) const { return M[row]; }

    static FMatrix Transpose(const FMatrix& Mat)
    {
        XMMATRIX xm = Mat.ToXMMATRIX();
        XMMATRIX xmT = XMMatrixTranspose(xm);
        return FromXMMATRIX(xmT);
    }

    static float Determinant(const FMatrix& Mat)
    {
        XMMATRIX xm = Mat.ToXMMATRIX();
        XMVECTOR det = XMMatrixDeterminant(xm);
        return XMVectorGetX(det);
    }

    static FMatrix Inverse(const FMatrix& Mat)
    {
        XMMATRIX xm = Mat.ToXMMATRIX();
        XMVECTOR det;
        XMMATRIX xmInv = XMMatrixInverse(&det, xm);
        return FromXMMATRIX(xmInv);
    }

    static FMatrix CreateRotation(float roll, float pitch, float yaw)
    {
        float radRoll = XMConvertToRadians(roll);
        float radPitch = XMConvertToRadians(pitch);
        float radYaw = XMConvertToRadians(yaw);

        float cosRoll = cos(radRoll), sinRoll = sin(radRoll);
        float cosPitch = cos(radPitch), sinPitch = sin(radPitch);
        float cosYaw = cos(radYaw), sinYaw = sin(radYaw);

        // Z축 (Yaw) 회전
        FMatrix rotationZ = { {
            { cosYaw, sinYaw, 0, 0 },
            { -sinYaw, cosYaw, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        } };

        // Y축 (Pitch) 회전
        FMatrix rotationY = { {
            { cosPitch, 0, -sinPitch, 0 },
            { 0, 1, 0, 0 },
            { sinPitch, 0, cosPitch, 0 },
            { 0, 0, 0, 1 }
        } };

        // X축 (Roll) 회전
        FMatrix rotationX = { {
            { 1, 0, 0, 0 },
            { 0, cosRoll, sinRoll, 0 },
            { 0, -sinRoll, cosRoll, 0 },
            { 0, 0, 0, 1 }
        } };

        // DirectX 표준 순서: Z(Yaw) → Y(Pitch) → X(Roll)  
        return rotationX * rotationY * rotationZ;  // 이렇게 하면  오른쪽 부터 적용됨
    }

    static FMatrix CreateScale(float scaleX, float scaleY, float scaleZ)
    {
        XMMATRIX xm = XMMatrixScaling(scaleX, scaleY, scaleZ);
        return FromXMMATRIX(xm);
    }

    static FMatrix CreateTranslationMatrix(const FVector& position)
    {
        XMMATRIX xm = XMMatrixTranslation(position.x, position.y, position.z);
        return FromXMMATRIX(xm);
    }

    FVector TransformPosition(const FVector& vector) const
    {
        XMVECTOR vec = XMVectorSet(vector.x, vector.y, vector.z, 1.0f);
        XMMATRIX xm = this->ToXMMATRIX();
        XMVECTOR transformed = XMVector4Transform(vec, xm);
        XMFLOAT4 out;
        XMStoreFloat4(&out, transformed);
        return out.w != 0.0f ? FVector(out.x / out.w, out.y / out.w, out.z / out.w)
                             : FVector(out.x, out.y, out.z);
    }

    static FVector TransformVector(const FVector& v, const FMatrix& m)
    {
        XMMATRIX xm = m.ToXMMATRIX();
        XMVECTOR vec = XMVectorSet(v.x, v.y, v.z, 0.0f);
        XMVECTOR transformed = XMVector3Transform(vec, xm);
        XMFLOAT3 out;
        XMStoreFloat3(&out, transformed);
        return FVector(out.x, out.y, out.z);
    }

    static FVector4 TransformVector(const FVector4& v, const FMatrix& m)
    {
        XMMATRIX xm = m.ToXMMATRIX();
        XMVECTOR vec = XMVectorSet(v.x, v.y, v.z, v.a);
        XMVECTOR transformed = XMVector4Transform(vec, xm);
        XMFLOAT4 out;
        XMStoreFloat4(&out, transformed);
        return FVector4(out.x, out.y, out.z, out.w);
    }

    DirectX::XMMATRIX ToXMMATRIX() const
    {
        // row-major 순서로 행렬을 구성
        return XMMatrixSet(
            M[0][0], M[0][1], M[0][2], M[0][3],
            M[1][0], M[1][1], M[1][2], M[1][3],
            M[2][0], M[2][1], M[2][2], M[2][3],
            M[3][0], M[3][1], M[3][2], M[3][3]
        );
    }

    static FMatrix FromXMMATRIX(const DirectX::XMMATRIX& xm)
    {
        FMatrix result;
        XMFLOAT4X4 tmp;
        XMStoreFloat4x4(&tmp, xm);
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.M[i][j] = tmp.m[i][j];
            }
        }
        return result;
    }
};
