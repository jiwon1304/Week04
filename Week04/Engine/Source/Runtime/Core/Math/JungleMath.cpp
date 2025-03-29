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

FQuat JungleMath::EulerToQuaternion(const FVector & eulerDegrees)
{
    float radRoll = DegToRad(eulerDegrees.x);
    float radPitch = DegToRad(eulerDegrees.y);
    float radYaw = DegToRad(eulerDegrees.z);
    XMVECTOR q = XMQuaternionRotationRollPitchYaw(radRoll, radPitch, radYaw);
    return FQuat::FromSIMD(q);
}

FVector JungleMath::QuaternionToEuler(const FQuat & quat)
{
    FQuat q = quat;
    q = q.Normalize();
    
    FVector euler;
    
    float sinYaw = 2.0f * (q.w * q.z + q.x * q.y);
    float cosYaw = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    euler.z = RadToDeg(atan2(sinYaw, cosYaw));
    
    float sinPitch = 2.0f * (q.w * q.y - q.z * q.x);
    if (fabs(sinPitch) >= 1.0f)
    {
        euler.y = RadToDeg(copysign(3.14159265359f / 2.0f, sinPitch));
    }
    else
    {
        euler.y = RadToDeg(asin(sinPitch));
    }
    
    float sinRoll = 2.0f * (q.w * q.x + q.y * q.z);
    float cosRoll = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    euler.x = RadToDeg(atan2(sinRoll, cosRoll));
    
    return euler;
}

FVector JungleMath::FVectorRotate(FVector & origin, const FQuat & rotation)
{
    return rotation.RotateVector(origin);
}

FMatrix JungleMath::CreateRotationMatrix(FVector rotation)
{
    float radRoll = DegToRad(rotation.x);
    float radPitch = DegToRad(rotation.y);
    float radYaw = DegToRad(rotation.z);
    XMVECTOR q = XMQuaternionRotationRollPitchYaw(radRoll, radPitch, radYaw);
    XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(q);
    return FMatrix::FromXMMATRIX(rotationMatrix);
}

float JungleMath::RadToDeg(float radian)
{
    return radian * (180.0f / 3.14159265359f);
}

float JungleMath::DegToRad(float degree)
{
    return degree * (3.14159265359f / 180.0f);
}
