#include "Engine/Source/Runtime/Core/Math/JungleMath.h"
#include <DirectXMath.h>
#include "MathUtility.h"

using namespace DirectX;

FVector4 JungleMath::ConvertV3ToV4(FVector vec3)
{
    XMVECTOR v = XMVectorSet(vec3.x, vec3.y, vec3.z, 0.0f);
    XMFLOAT4 result;
    XMStoreFloat4(&result, v);
    return FVector4(result.x, result.y, result.z, result.w);
}

FMatrix JungleMath::CreateModelMatrix(FVector translation, FVector rotation, FVector scale)
{
    FMatrix Translation = FMatrix::CreateTranslationMatrix(translation);
    FMatrix Rotation = FMatrix::CreateRotation(rotation.x, rotation.y, rotation.z);
    FMatrix Scale = FMatrix::CreateScale(scale.x, scale.y, scale.z);
    return Scale * Rotation * Translation;
}

FMatrix JungleMath::CreateModelMatrix(FVector translation, FQuat rotation, FVector scale)
{
    FMatrix Translation = FMatrix::CreateTranslationMatrix(translation);
    FMatrix Rotation = rotation.ToMatrix();
    FMatrix Scale = FMatrix::CreateScale(scale.x, scale.y, scale.z);
    return Scale * Rotation * Translation;
}

FMatrix JungleMath::CreateViewMatrix(FVector eye, FVector target, FVector up)
{
    XMVECTOR eyeVec = XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
    XMVECTOR targetVec = XMVectorSet(target.x, target.y, target.z, 1.0f);
    XMVECTOR upVec = XMVectorSet(up.x, up.y, up.z, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyeVec, targetVec, upVec);
    return FMatrix::FromXMMATRIX(view);
}

FMatrix JungleMath::CreateProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane)
{
    // fov는 라디안 단위라고 가정합니다.
    XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, aspect, nearPlane, farPlane);
    return FMatrix::FromXMMATRIX(proj);
}

FMatrix JungleMath::CreateOrthoProjectionMatrix(float width, float height, float nearPlane, float farPlane)
{
    float r = width * 0.5f;
    float t = height * 0.5f;
    XMMATRIX ortho = XMMatrixOrthographicOffCenterLH(-r, r, -t, t, nearPlane, farPlane);
    return FMatrix::FromXMMATRIX(ortho);
}

FVector JungleMath::FVectorRotate(FVector & origin, const FVector & rotation)
{
    FQuat quaternion = JungleMath::EulerToQuaternion(rotation);
    return quaternion.RotateVector(origin);
}

FQuat JungleMath::EulerToQuaternion(const FVector& eulerDegrees)
{
    float yaw = XMConvertToRadians(eulerDegrees.z);   // Z축 Yaw
    float pitch = XMConvertToRadians(eulerDegrees.y); // Y축 Pitch
    float roll = XMConvertToRadians(eulerDegrees.x);  // X축 Roll

    float halfYaw = yaw * 0.5f;
    float halfPitch = pitch * 0.5f;
    float halfRoll = roll * 0.5f;

    float cosYaw = cos(halfYaw);
    float sinYaw = sin(halfYaw);
    float cosPitch = cos(halfPitch);
    float sinPitch = sin(halfPitch);
    float cosRoll = cos(halfRoll);
    float sinRoll = sin(halfRoll);

    FQuat quat;
    quat.w = cosYaw * cosPitch * cosRoll + sinYaw * sinPitch * sinRoll;
    quat.x = cosYaw * cosPitch * sinRoll - sinYaw * sinPitch * cosRoll;
    quat.y = cosYaw * sinPitch * cosRoll + sinYaw * cosPitch * sinRoll;
    quat.z = sinYaw * cosPitch * cosRoll - cosYaw * sinPitch * sinRoll;

    return quat.Normalize();
}

FVector JungleMath::QuaternionToEuler(const FQuat& quat)
{
    FVector euler;

    // 쿼터니언 정규화
    FQuat q = quat;
    q = q.Normalize();

    // Yaw (Z 축 회전)
    float sinYaw = 2.0f * (q.w * q.z + q.x * q.y);
    float cosYaw = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.z = XMConvertToDegrees(atan2(sinYaw, cosYaw));

    // Pitch (Y 축 회전, 짐벌락 방지)
    float sinPitch = 2.0f * (q.w * q.y - q.z * q.x);
    if (fabs(sinPitch) >= 1.0f)
    {
        euler.y = XMConvertToDegrees(static_cast<float>(copysign(PI / 2, sinPitch))); // 🔥 Gimbal Lock 방지
    }
    else
    {
        euler.y = XMConvertToDegrees(asin(sinPitch));
    }

    // Roll (X 축 회전)
    float sinRoll = 2.0f * (q.w * q.x + q.y * q.z);
    float cosRoll = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler.x = XMConvertToDegrees(atan2(sinRoll, cosRoll));

    return euler;
}

FVector JungleMath::FVectorRotate(FVector & origin, const FQuat & rotation)
{
    return rotation.RotateVector(origin);
}

FMatrix JungleMath::CreateRotationMatrix(FVector rotation)
{
    XMVECTOR quatX = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XMConvertToRadians(rotation.x));
    XMVECTOR quatY = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(rotation.y));
    XMVECTOR quatZ = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(rotation.z));

    XMVECTOR rotationQuat = XMQuaternionMultiply(quatZ, XMQuaternionMultiply(quatY, quatX));
    rotationQuat = XMQuaternionNormalize(rotationQuat);  // 정규화 필수

    XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(rotationQuat);
    FMatrix result = FMatrix::Identity;  // 기본값 설정 (단위 행렬)

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            result.M[i][j] = rotationMatrix.r[i].m128_f32[j];  // XMMATRIX에서 FMatrix로 값 복사
        }
    }
    return result;
}
