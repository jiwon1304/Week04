#pragma once

#include <DirectXMath.h>
#include "MathUtility.h"

struct FQuat
{
    float w, x, y, z;

    // 기본 생성자
    FQuat() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}

    // FQuat 생성자 추가: 회전 축과 각도를 받아서 FQuat 생성
    FQuat(const FVector& Axis, float Angle)
    {
        float HalfAngle = Angle * 0.5f;
        float SinHalfAngle = sinf(HalfAngle);
        float CosHalfAngle = cosf(HalfAngle);
        x = Axis.x * SinHalfAngle;
        y = Axis.y * SinHalfAngle;
        z = Axis.z * SinHalfAngle;
        w = CosHalfAngle;
    }

    // w, x, y, z 값으로 초기화
    FQuat(float InW, float InX, float InY, float InZ) : w(InW), x(InX), y(InY), z(InZ) {}

    // Helper: FQuat을 XMVECTOR로 변환 (DirectXMath는 (x,y,z,w) 순서를 사용)
    inline DirectX::XMVECTOR ToSIMD() const
    {
        return DirectX::XMVectorSet(x, y, z, w);
    }

    // Helper: XMVECTOR에서 FQuat으로 변환 (벡터 순서: (x,y,z,w))
    static inline FQuat FromSIMD(DirectX::XMVECTOR v)
    {
        FQuat q;
        q.x = v.m128_f32[0];
        q.y = v.m128_f32[1];
        q.z = v.m128_f32[2];
        q.w = v.m128_f32[3];
        return q;
    }

    // 쿼터니언의 곱셈 연산 (회전 결합)
    FQuat operator*(const FQuat& Other) const
    {
        DirectX::XMVECTOR q1 = this->ToSIMD();
        DirectX::XMVECTOR q2 = Other.ToSIMD();
        DirectX::XMVECTOR result = DirectX::XMQuaternionMultiply(q1, q2);
        return FQuat::FromSIMD(result);
    }

    // (쿼터니언) 벡터 회전
    FVector RotateVector(const FVector& Vec) const
    {
        DirectX::XMVECTOR q = this->ToSIMD();
        DirectX::XMVECTOR v = DirectX::XMVectorSet(Vec.x, Vec.y, Vec.z, 0.0f);
        DirectX::XMVECTOR rotated = DirectX::XMVector3Rotate(v, q);
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out, rotated);
        return FVector(out.x, out.y, out.z);
    }

    // 단위 쿼터니언 여부 확인
    bool IsNormalized() const
    {
        DirectX::XMVECTOR q = this->ToSIMD();
        float length = DirectX::XMVectorGetX(DirectX::XMVector4Length(q));
        return fabsf(length - 1.0f) < SMALL_NUMBER;
    }

    // 쿼터니언 정규화 (단위 쿼터니언으로 만듬)
    FQuat Normalize() const
    {
        DirectX::XMVECTOR q = this->ToSIMD();
        DirectX::XMVECTOR norm = DirectX::XMQuaternionNormalize(q);
        return FQuat::FromSIMD(norm);
    }

    // 회전 각도와 축으로부터 쿼터니언 생성 (axis-angle 방식)
    static FQuat FromAxisAngle(const FVector& Axis, float Angle)
    {
        DirectX::XMVECTOR axis = DirectX::XMVectorSet(Axis.x, Axis.y, Axis.z, 0.0f);
        DirectX::XMVECTOR q = DirectX::XMQuaternionRotationAxis(axis, Angle);
        return FQuat::FromSIMD(q);
    }

    // 오일러 각(roll, pitch, yaw; 단위: 도)로부터 회전 쿼터니언 생성
    static FQuat CreateRotation(float roll, float pitch, float yaw)
    {
        // 각도를 라디안으로 변환
        float radRoll = roll * (3.14159265359f / 180.0f);
        float radPitch = pitch * (3.14159265359f / 180.0f);
        float radYaw = yaw * (3.14159265359f / 180.0f);

        // 각 축에 대한 회전 쿼터니언 계산
        FQuat qRoll = FQuat(FVector(1.0f, 0.0f, 0.0f), radRoll);  // X축 회전
        FQuat qPitch = FQuat(FVector(0.0f, 1.0f, 0.0f), radPitch);  // Y축 회전
        FQuat qYaw = FQuat(FVector(0.0f, 0.0f, 1.0f), radYaw);  // Z축 회전

        // 회전 순서대로 쿼터니언 결합 (Y -> X -> Z)
        return qRoll * qPitch * qYaw;
    }

    // 쿼터니언을 회전 행렬로 변환
    FMatrix ToMatrix() const
    {
        DirectX::XMVECTOR q = this->ToSIMD();
        DirectX::XMMATRIX m = DirectX::XMMatrixRotationQuaternion(q);
        DirectX::XMFLOAT4X4 mat;
        DirectX::XMStoreFloat4x4(&mat, m);
        FMatrix RotationMatrix;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                RotationMatrix.M[i][j] = mat.m[i][j];
            }
        }
        return RotationMatrix;
    }
};
