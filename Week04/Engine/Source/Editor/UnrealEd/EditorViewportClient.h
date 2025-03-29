#pragma once
#include <sstream>

#include "Define.h"
#include "Container/Map.h"
#include "UObject/ObjectMacros.h"
#include "ViewportClient.h"
#include "EngineLoop.h"
#include "EngineBaseTypes.h"
#include "Math/MathUtility.h"

#define MIN_ORTHOZOOM				1.0							/* 2D ortho viewport zoom >= MIN_ORTHOZOOM */
#define MAX_ORTHOZOOM				1e25	

extern FEngineLoop GEngineLoop;

struct FViewportCameraTransform
{
private:

public:

    FVector GetForwardVector();
    FVector GetRightVector();
    FVector GetUpVector();

public:
    FViewportCameraTransform();

    /** Sets the transform's location */
    void SetLocation(const FVector& Position)
    {
        ViewLocation = Position;
    }

    /** Sets the transform's rotation */
    void SetRotation(const FVector& Rotation)
    {
        ViewRotation = Rotation;
    }

    /** Sets the location to look at during orbit */
    void SetLookAt(const FVector& InLookAt)
    {
        LookAt = InLookAt;
    }

    /** Set the ortho zoom amount */
    void SetOrthoZoom(float InOrthoZoom)
    {
        assert(InOrthoZoom >= MIN_ORTHOZOOM && InOrthoZoom <= MAX_ORTHOZOOM);
        OrthoZoom = InOrthoZoom;
    }

    /** Check if transition curve is playing. */
 /*    bool IsPlaying();*/

    /** @return The transform's location */
    FORCEINLINE const FVector& GetLocation() const { return ViewLocation; }

    /** @return The transform's rotation */
    FORCEINLINE const FVector& GetRotation() const { return ViewRotation; }

    /** @return The look at point for orbiting */
    FORCEINLINE const FVector& GetLookAt() const { return LookAt; }

    /** @return The ortho zoom amount */
    FORCEINLINE float GetOrthoZoom() const { return OrthoZoom; }

public:
    /** Current viewport Position. */
    FVector	ViewLocation;
    /** Current Viewport orientation; valid only for perspective projections. */
    FVector ViewRotation;
    FVector	DesiredLocation;
    /** When orbiting, the point we are looking at */
    FVector LookAt;
    /** Viewport start location when animating to another location */
    FVector StartLocation;
    /** Ortho zoom amount */
    float OrthoZoom;
};

struct Plane {
    FVector normal;
    float d;
};

struct Frustum {
    Plane planes[6]; //좌우 상하 near far

    void CreatePlane(FViewportCameraTransform camera, float fov, float nearZ, float farZ, float aspectRatio) {
        FVector normal;
        float d;

        FVector point = camera.GetLocation();
        FVector forward = camera.GetForwardVector();
        FVector up = camera.GetUpVector();
        FVector right = camera.GetRightVector();

        fov = fov * PI / 180.0f;
        float halfFOV = fov / 2.0f;
        float tanFOV = tanf(halfFOV);
        float nearHeight = tanFOV * nearZ;
        float nearWidth = nearHeight * aspectRatio;
        float farHeight = tanFOV * farZ;
        float farWidth = farHeight * aspectRatio;
        FVector nearPoint = point + forward * nearZ;
        FVector farPoint = point + forward * farZ;

        FVector leftNear = (nearPoint - right * nearWidth) - point;
        FVector leftFar = (farPoint - right * farWidth) - point;
        FVector leftDir = leftFar - leftNear;
        normal = up.Cross(leftDir).Normalize();
        d = -1.0f * normal.Dot(point);
        planes[0].normal = normal;
        planes[0].d = d;

        FVector rightNear = (nearPoint + right * nearWidth) - point;
        FVector rightFar = (farPoint + right * farWidth) - point;
        FVector rightDir = rightFar - rightNear;
        normal = rightDir.Cross(up).Normalize();
        d = -1.0f * normal.Dot(point);
        planes[1].normal = normal;
        planes[1].d = d;

        FVector upNear = (nearPoint + up * nearHeight) - point;
        FVector upFar = (farPoint + up * farHeight) - point;
        FVector upDir = upFar - upNear;
        normal = right.Cross(upDir).Normalize();
        d = -1.0f * normal.Dot(point);
        planes[2].normal = normal;
        planes[2].d = d;

        FVector downNear = (nearPoint - up * nearHeight) - point;
        FVector downFar = (farPoint - up * farHeight) - point;
        FVector downDir = downFar - downNear;
        normal = downDir.Cross(right).Normalize();
        d = -1.0f * normal.Dot(point);
        planes[3].normal = normal;
        planes[3].d = d;

        normal = forward;
        point = nearPoint;
        d = -1.0f * normal.Dot(point);
        planes[4].normal = normal;
        planes[4].d = d;

        normal = forward * -1.0f;
        point = farPoint;
        d = -1.0f * normal.Dot(point);
        planes[5].normal = normal;
        planes[5].d = d;
    }

    FVector IntersectThreePlanes(const Plane& p1, const Plane& p2, const Plane& p3)
    {
        const FVector& n1 = p1.normal;
        const FVector& n2 = p2.normal;
        const FVector& n3 = p3.normal;

        float det = n1.Dot(n2.Cross(n3));
        if (abs(det) < KINDA_SMALL_NUMBER)
        {
            return FVector::ZeroVector; // 평면들이 평행하거나 일치 → 교점 없음
        }

        FVector result =
            (n2.Cross(n3) * (-p1.d) +
                n3.Cross(n1) * (-p2.d) +
                n1.Cross(n2) * (-p3.d)) * (1.0f / det);

        return result;
    }

    TArray<FVector> ExtractFrustumCorners()
    {
        enum { Left = 0, Right, Bottom, Top, Near, Far };

        TArray<FVector> outCorners;

        // Near plane corners
        outCorners.Add(IntersectThreePlanes(planes[Left], planes[Bottom], planes[Near])); // Near Bottom Left
        outCorners.Add(IntersectThreePlanes(planes[Left], planes[Top], planes[Near])); // Near Top Left
        outCorners.Add(IntersectThreePlanes(planes[Right], planes[Bottom], planes[Near])); // Near Bottom Right
        outCorners.Add(IntersectThreePlanes(planes[Right], planes[Top], planes[Near])); // Near Top Right

        // Far plane corners
        outCorners.Add(IntersectThreePlanes(planes[Left], planes[Bottom], planes[Far])); // Far Bottom Left
        outCorners.Add(IntersectThreePlanes(planes[Left], planes[Top], planes[Far])); // Far Top Left
        outCorners.Add(IntersectThreePlanes(planes[Right], planes[Bottom], planes[Far])); // Far Bottom Right
        outCorners.Add(IntersectThreePlanes(planes[Right], planes[Top], planes[Far])); // Far Top Right

        return outCorners;
    }
};

class FEditorViewportClient : public FViewportClient
{
public:
    FEditorViewportClient();
    ~FEditorViewportClient();

    virtual void        Draw(FViewport* Viewport) override;
    virtual UWorld*     GetWorld() const { return NULL; };
    void Initialize(int32 viewportIndex);
    void Tick(float DeltaTime);
    void Release();

    void Input();
    void ResizeViewport(const DXGI_SWAP_CHAIN_DESC& swapchaindesc);
    void ResizeViewport(FRect Top, FRect Bottom, FRect Left, FRect Right);

    bool IsSelected(POINT point);
protected:
    /** Camera speed setting */
    int32 CameraSpeedSetting = 1;
    /** Camera speed scalar */
    float CameraSpeedScalar = 1.0f;
    float GridSize;

public: 
    FViewport* Viewport;
    int32 ViewportIndex;
    FViewport* GetViewport() { return Viewport; }
    D3D11_VIEWPORT& GetD3DViewport();


public:
    //카메라
    /** Viewport camera transform data for perspective viewports */
    FViewportCameraTransform ViewTransformPerspective;
    FViewportCameraTransform ViewTransformOrthographic;
    // 카메라 정보 
    float ViewFOV = 60.0f;
    float AspectRatio;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    static FVector Pivot;
    static float orthoSize;
    ELevelViewportType ViewportType;
    uint64 ShowFlag;
    EViewModeIndex ViewMode;

    FMatrix View;
    FMatrix Projection;

    Frustum CameraFrustum;
    
public: //Camera Movement
    void CameraMoveForward(float _Value);
    void CameraMoveRight(float _Value);
    void CameraMoveUp(float _Value);
    void CameraRotateYaw(float _Value);
    void CameraRotatePitch(float _Value);
    void PivotMoveRight(float _Value);
    void PivotMoveUp(float _Value);

    FMatrix& GetViewMatrix() { return  View; }
    FMatrix& GetProjectionMatrix() { return Projection; }
    Frustum& GetFrustum() { return CameraFrustum; }
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    void UpdateFrustum();

    bool IsOrtho() const;
    bool IsPerspective() const;
    ELevelViewportType GetViewportType() const;
    void SetViewportType(ELevelViewportType InViewportType);
    void UpdateOrthoCameraLoc();
    EViewModeIndex GetViewMode() { return ViewMode; }
    void SetViewMode(EViewModeIndex newMode) { ViewMode = newMode; }
    uint64 GetShowFlag() { return ShowFlag; }
    void SetShowFlag(uint64 newMode) { ShowFlag = newMode; }
    bool GetIsOnRBMouseClick() { return bRightMouseDown; }

    //Flag Test Code
    static void SetOthoSize(float _Value);
private: // Input
    POINT lastMousePos;
    bool bRightMouseDown = false;
    bool bCameraMoved = false;

public:
    void LoadConfig(const TMap<FString, FString>& config);
    void SaveConfig(TMap<FString, FString>& config);
private:
    TMap<FString, FString> ReadIniFile(const FString& filePath);
    void WriteIniFile(const FString& filePath, const TMap<FString, FString>& config);
	
public:
    PROPERTY(int32, CameraSpeedSetting)
    PROPERTY(float, GridSize)
    float GetCameraSpeedScalar() const { return CameraSpeedScalar; };
    void SetCameraSpeedScalar(float value);

private:
    template <typename T>
    T GetValueFromConfig(const TMap<FString, FString>& config, const FString& key, T defaultValue) {
        if (const FString* Value = config.Find(key))
        {
            std::istringstream iss(**Value);
            T value;
            if (iss >> value)
            {
                return value;
            }
        }
        return defaultValue;
    }
};