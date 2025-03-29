#pragma once

#include <DirectXMath.h>
#include "Vector4.h"
#include "Vector.h"

// 4x4 행렬 연산
struct FMatrix
{
    float M[4][4];
    static const FMatrix Identity;

    FMatrix operator+(const FMatrix& Other) const
    {
        DirectX::XMMATRIX xm1 = this->ToXMMATRIX();
        DirectX::XMMATRIX xm2 = Other.ToXMMATRIX();
        DirectX::XMMATRIX xmOut(
            DirectX::XMVectorAdd(xm1.r[0], xm2.r[0]),
            DirectX::XMVectorAdd(xm1.r[1], xm2.r[1]),
            DirectX::XMVectorAdd(xm1.r[2], xm2.r[2]),
            DirectX::XMVectorAdd(xm1.r[3], xm2.r[3])
        );
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator-(const FMatrix& Other) const
    {
        DirectX::XMMATRIX xm1 = this->ToXMMATRIX();
        DirectX::XMMATRIX xm2 = Other.ToXMMATRIX();
        DirectX::XMMATRIX xmOut(
            DirectX::XMVectorSubtract(xm1.r[0], xm2.r[0]),
            DirectX::XMVectorSubtract(xm1.r[1], xm2.r[1]),
            DirectX::XMVectorSubtract(xm1.r[2], xm2.r[2]),
            DirectX::XMVectorSubtract(xm1.r[3], xm2.r[3])
        );
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator*(const FMatrix& Other) const
    {
        DirectX::XMMATRIX xm1 = this->ToXMMATRIX();
        DirectX::XMMATRIX xm2 = Other.ToXMMATRIX();
        DirectX::XMMATRIX xmOut = DirectX::XMMatrixMultiply(xm1, xm2);
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator*(float Scalar) const
    {
        DirectX::XMMATRIX xm = this->ToXMMATRIX();
        DirectX::XMVECTOR s = DirectX::XMVectorReplicate(Scalar);
        DirectX::XMMATRIX xmOut(
            DirectX::XMVectorMultiply(xm.r[0], s),
            DirectX::XMVectorMultiply(xm.r[1], s),
            DirectX::XMVectorMultiply(xm.r[2], s),
            DirectX::XMVectorMultiply(xm.r[3], s)
        );
        return FromXMMATRIX(xmOut);
    }

    FMatrix operator/(float Scalar) const
    {
        float inv = 1.0f / Scalar;
        DirectX::XMMATRIX xm = this->ToXMMATRIX();
        DirectX::XMVECTOR invS = DirectX::XMVectorReplicate(inv);
        DirectX::XMMATRIX xmOut(
            DirectX::XMVectorMultiply(xm.r[0], invS),
            DirectX::XMVectorMultiply(xm.r[1], invS),
            DirectX::XMVectorMultiply(xm.r[2], invS),
            DirectX::XMVectorMultiply(xm.r[3], invS)
        );
        return FromXMMATRIX(xmOut);
    }

    float* operator[](int row)
    {
        return M[row];
    }

    const float* operator[](int row) const
    {
        return M[row];
    }

    static FMatrix Transpose(const FMatrix& Mat)
    {
        DirectX::XMMATRIX xm = Mat.ToXMMATRIX();
        DirectX::XMMATRIX xmT = DirectX::XMMatrixTranspose(xm);
        return FromXMMATRIX(xmT);
    }

    static float Determinant(const FMatrix& Mat)
    {
        DirectX::XMMATRIX xm = Mat.ToXMMATRIX();
        DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(xm);
        return DirectX::XMVectorGetX(det);
    }

    static FMatrix Inverse(const FMatrix& Mat)
    {
        DirectX::XMMATRIX xm = Mat.ToXMMATRIX();
        DirectX::XMVECTOR det;
        DirectX::XMMATRIX xmInv = DirectX::XMMatrixInverse(&det, xm);
        return FromXMMATRIX(xmInv);
    }

    static FMatrix CreateRotation(float roll, float pitch, float yaw)
    {
        float radRoll  = roll  * (3.14159265359f / 180.0f);
        float radPitch = pitch * (3.14159265359f / 180.0f);
        float radYaw   = yaw   * (3.14159265359f / 180.0f);
        DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(radRoll, radPitch, radYaw);
        DirectX::XMMATRIX xm = DirectX::XMMatrixRotationQuaternion(q);
        return FromXMMATRIX(xm);
    }

    static FMatrix CreateScale(float scaleX, float scaleY, float scaleZ)
    {
        DirectX::XMMATRIX xm = DirectX::XMMatrixScaling(scaleX, scaleY, scaleZ);
        return FromXMMATRIX(xm);
    }

    FVector TransformPosition(const FVector& vector) const
    {
        DirectX::XMVECTOR vec = DirectX::XMVectorSet(vector.x, vector.y, vector.z, 1.0f);
        DirectX::XMMATRIX xm = this->ToXMMATRIX();
        DirectX::XMVECTOR transformed = DirectX::XMVector4Transform(vec, xm);
        DirectX::XMFLOAT4 out;
        DirectX::XMStoreFloat4(&out, transformed);
        if (out.w != 0.0f)
        {
            return FVector(out.x / out.w, out.y / out.w, out.z / out.w);
        }
        else
        {
            return FVector(out.x, out.y, out.z);
        }
    }

    static FVector TransformVector(const FVector& v, const FMatrix& m)
    {
        DirectX::XMMATRIX xm = m.ToXMMATRIX();
        DirectX::XMVECTOR vec = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
        DirectX::XMVECTOR transformed = DirectX::XMVector3Transform(vec, xm);
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out, transformed);
        return FVector(out.x, out.y, out.z);
    }

    static FVector4 TransformVector(const FVector4& v, const FMatrix& m)
    {
        DirectX::XMMATRIX xm = m.ToXMMATRIX();
        DirectX::XMVECTOR vec = DirectX::XMVectorSet(v.x, v.y, v.z, v.a);
        DirectX::XMVECTOR transformed = DirectX::XMVector4Transform(vec, xm);
        DirectX::XMFLOAT4 out;
        DirectX::XMStoreFloat4(&out, transformed);
        return FVector4(out.x, out.y, out.z, out.w);
    }

    static FMatrix CreateTranslationMatrix(const FVector& position)
    {
        DirectX::XMMATRIX xm = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
        return FromXMMATRIX(xm);
    }

    DirectX::XMMATRIX ToXMMATRIX() const
    {
        // row-major 방식: FMatrix의 M[i][j]가 행 i, 열 j에 해당
        return DirectX::XMMatrixSet(
            M[0][0], M[0][1], M[0][2], M[0][3],
            M[1][0], M[1][1], M[1][2], M[1][3],
            M[2][0], M[2][1], M[2][2], M[2][3],
            M[3][0], M[3][1], M[3][2], M[3][3]
        );
    }

    static FMatrix FromXMMATRIX(const DirectX::XMMATRIX& xm)
    {
        FMatrix result;
        DirectX::XMFLOAT4X4 tmp;
        DirectX::XMStoreFloat4x4(&tmp, xm);
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
