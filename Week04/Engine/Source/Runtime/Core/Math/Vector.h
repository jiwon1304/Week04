#pragma once

#include <DirectXMath.h>

struct FVector2D
{
	float x,y;
	FVector2D(float _x = 0, float _y = 0) : x(_x), y(_y) {}

    FVector2D operator+(const FVector2D& rhs) const
	{
	    DirectX::XMFLOAT2 out;
	    DirectX::XMStoreFloat2(&out,
            DirectX::XMVectorAdd(
                DirectX::XMVectorSet(x, y, 0.0f, 0.0f),
                DirectX::XMVectorSet(rhs.x, rhs.y, 0.0f, 0.0f)
            ));
	    return FVector2D(out.x, out.y);
	}
	
    FVector2D operator-(const FVector2D& rhs) const
	{
	    DirectX::XMFLOAT2 out;
	    DirectX::XMStoreFloat2(&out,
            DirectX::XMVectorSubtract(
                DirectX::XMVectorSet(x, y, 0.0f, 0.0f),
                DirectX::XMVectorSet(rhs.x, rhs.y, 0.0f, 0.0f)
            ));
	    return FVector2D(out.x, out.y);
	}
    
    FVector2D operator*(float rhs) const
	{
	    DirectX::XMFLOAT2 out;
	    DirectX::XMStoreFloat2(&out,
            DirectX::XMVectorScale(
                DirectX::XMVectorSet(x, y, 0.0f, 0.0f),
                rhs));
	    return FVector2D(out.x, out.y);
	}

    FVector2D operator/(float rhs) const
	{
	    DirectX::XMFLOAT2 out;
	    DirectX::XMStoreFloat2(&out,
            DirectX::XMVectorScale(
                DirectX::XMVectorSet(x, y, 0.0f, 0.0f),
                1.0f / rhs));
	    return FVector2D(out.x, out.y);
	}

    FVector2D& operator+=(const FVector2D& rhs)
	{
	    *this = *this + rhs;
	    return *this;
	}
};

// 3D 벡터
struct FVector
{
    float x, y, z;
    FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

    FVector operator+(const FVector& other) const
    {
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out,
            DirectX::XMVectorAdd(
                DirectX::XMVectorSet(x, y, z, 0.0f),
                DirectX::XMVectorSet(other.x, other.y, other.z, 0.0f)
            ));
        return FVector(out.x, out.y, out.z);
    }

    FVector operator-(const FVector& other) const
    {
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out,
            DirectX::XMVectorSubtract(
                DirectX::XMVectorSet(x, y, z, 0.0f),
                DirectX::XMVectorSet(other.x, other.y, other.z, 0.0f)
            ));
        return FVector(out.x, out.y, out.z);
    }

    float Dot(const FVector& other) const
    {
        DirectX::XMVECTOR v1 = DirectX::XMVectorSet(x, y, z, 0.0f);
        DirectX::XMVECTOR v2 = DirectX::XMVectorSet(other.x, other.y, other.z, 0.0f);
        return DirectX::XMVectorGetX(DirectX::XMVector3Dot(v1, v2));
    }

    float Magnitude() const
    {
        DirectX::XMVECTOR v = DirectX::XMVectorSet(x, y, z, 0.0f);
        return DirectX::XMVectorGetX(DirectX::XMVector3Length(v));
    }

    FVector Normalize() const
    {
        DirectX::XMVECTOR v = DirectX::XMVectorSet(x, y, z, 0.0f);
        DirectX::XMVECTOR norm = DirectX::XMVector3Normalize(v);
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out, norm);
        return FVector(out.x, out.y, out.z);
    }

    FVector Cross(const FVector& other) const
    {
        DirectX::XMVECTOR v1 = DirectX::XMVectorSet(x, y, z, 0.0f);
        DirectX::XMVECTOR v2 = DirectX::XMVectorSet(other.x, other.y, other.z, 0.0f);
        DirectX::XMVECTOR cross = DirectX::XMVector3Cross(v1, v2);
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out, cross);
        return FVector(out.x, out.y, out.z);
    }

    FVector operator*(float scalar) const
    {
        DirectX::XMFLOAT3 out;
        DirectX::XMStoreFloat3(&out,
            DirectX::XMVectorScale(
                DirectX::XMVectorSet(x, y, z, 0.0f),
                scalar));
        return FVector(out.x, out.y, out.z);
    }

    bool operator==(const FVector& other) const
    {
        return (x == other.x && y == other.y && z == other.z);
    }

    float Distance(const FVector& other) const
    {
        return ((*this - other).Magnitude());
    }

    DirectX::XMFLOAT3 ToXMFLOAT3() const
    {
        return DirectX::XMFLOAT3(x, y, z);
    }

    static const FVector ZeroVector;
    static const FVector OneVector;
    static const FVector UpVector;
    static const FVector ForwardVector;
    static const FVector RightVector;
};
