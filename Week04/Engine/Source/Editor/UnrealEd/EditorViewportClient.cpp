#include "EditorViewportClient.h"
#include "fstream"
#include "sstream"
#include "ostream"
#include "Math/JungleMath.h"
#include "EngineLoop.h"
#include "UnrealClient.h"
#include "World.h"
#include "GameFramework/Actor.h"

FVector FEditorViewportClient::Pivot = FVector(0.0f, 0.0f, 0.0f);
float FEditorViewportClient::orthoSize = 10.0f;

FEditorViewportClient::FEditorViewportClient()
    : Viewport(nullptr), ViewMode(VMI_Lit), ViewportType(LVT_Perspective), ShowFlag(31)
{

}

FEditorViewportClient::~FEditorViewportClient()
{
    Release();
}

void FEditorViewportClient::Draw(FViewport* Viewport)
{
}

void FEditorViewportClient::Initialize(int32 viewportIndex)
{

    //ViewTransformPerspective.SetLocation(FVector(8.0f, 8.0f, 8.f));
    //ViewTransformPerspective.SetRotation(FVector(0.0f, 45.0f, -135.0f));
    Viewport = new FViewport(static_cast<EViewScreenLocation>(viewportIndex));
    ResizeViewport(GEngineLoop.GraphicDevice.SwapchainDesc);
    ViewportIndex = viewportIndex;
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    UpdateFrustum();
}

bool FEditorViewportClient::Tick(float DeltaTime)
{
    Input(DeltaTime);
    
    bool bReturn = bCameraMoved || bProjectionUpdated;
    if (bCameraMoved)
    {
        UpdateViewMatrix();
        UpdateFrustum();
        bCameraMoved = false;
    }
    if (bProjectionUpdated)
    {
        // 이 값은 플래그만 세팅해줌.
        bProjectionUpdated = false;
    }
    
    return bReturn;
}

void FEditorViewportClient::Release()
{
    if (Viewport)
        delete Viewport;
 
}

void FEditorViewportClient::Input(float DeltaTime)
{
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) // VK_RBUTTON은 마우스 오른쪽 버튼을 나타냄
    {
        if (!bRightMouseDown)
        {
            // 마우스 오른쪽 버튼을 처음 눌렀을 때, 마우스 위치 초기화
            GetCursorPos(&lastMousePos);
            bRightMouseDown = true;
        }
        else
        {
            // 마우스 이동량 계산
            POINT currentMousePos;
            GetCursorPos(&currentMousePos);

            // 마우스 이동 차이 계산
            int32 deltaX = currentMousePos.x - lastMousePos.x;
            int32 deltaY = currentMousePos.y - lastMousePos.y;

            if (deltaX != 0 || deltaY != 0)
            {
                // Yaw(좌우 회전) 및 Pitch(상하 회전) 값 변경
                if (IsPerspective()) {
                    CameraRotateYaw(deltaX * 0.1f);  // X 이동에 따라 좌우 회전
                    CameraRotatePitch(deltaY * 0.1f);  // Y 이동에 따라 상하 회전
                }
                else
                {
                    PivotMoveRight(deltaX);
                    PivotMoveUp(deltaY);
                }

                SetCursorPos(lastMousePos.x, lastMousePos.y);

                bCameraMoved = true;
            }
        }

        // Move
        FVector MoveDirection = FVector::ZeroVector;
        if (IsPerspective())
        {
            if (GetAsyncKeyState('A') & 0x8000)
            {
                MoveDirection = MoveDirection - ViewTransformPerspective.GetRightVector();
            }
            if (GetAsyncKeyState('D') & 0x8000)
            {
                MoveDirection = MoveDirection + ViewTransformPerspective.GetRightVector();
            }
            if (GetAsyncKeyState('W') & 0x8000)
            {
                MoveDirection = MoveDirection + ViewTransformPerspective.GetForwardVector();
            }
            if (GetAsyncKeyState('S') & 0x8000)
            {
                MoveDirection = MoveDirection - ViewTransformPerspective.GetForwardVector();
            }
            if (GetAsyncKeyState('E') & 0x8000)
            {
                MoveDirection = MoveDirection + FVector::UpVector;
            }
            if (GetAsyncKeyState('Q') & 0x8000)
            {
                MoveDirection = MoveDirection - FVector::UpVector;
            }
        }
        if (MoveDirection != FVector::ZeroVector)
        {
            const FVector CurrentLocation = ViewTransformPerspective.GetLocation();
            const FVector NextLocation = CurrentLocation + MoveDirection.Normalize() * CameraSpeedScalar * DeltaTime;
            ViewTransformPerspective.SetLocation(NextLocation);
            bCameraMoved = true;
        }
    }
    else
    {
        bRightMouseDown = false; // 마우스 오른쪽 버튼을 떼면 상태 초기화
    }

    if (GetAsyncKeyState('P') & 0x8000)
    {
        if (!bP)
        {
            bP = true;
            GEngineLoop.bIsInhibitorEnabled ^= true;
        }
    }
    else
    {
        bP = false;
    }
    
    return; // W04

    // Focus Selected Actor
    if (GetAsyncKeyState('F') & 0x8000)
    {
        if (AActor* PickedActor = GEngineLoop.GetWorld()->GetSelectedActor())
        {
            FViewportCameraTransform& ViewTransform = ViewTransformPerspective;
            ViewTransform.SetLocation(
                // TODO: 10.0f 대신, 정점의 min, max의 거리를 구해서 하면 좋을 듯
                PickedActor->GetActorLocation() - (ViewTransform.GetForwardVector() * 10.0f)
            );
        }
    }
}
void FEditorViewportClient::ResizeViewport(const DXGI_SWAP_CHAIN_DESC& swapchaindesc)
{
    if (Viewport) { 
        Viewport->ResizeViewport(swapchaindesc);    
    }
    else {
        UE_LOG(LogLevel::Error, "Viewport is nullptr");
    }
    AspectRatio = GEngineLoop.GetAspectRatio(GEngineLoop.GraphicDevice.SwapChain);
    UpdateProjectionMatrix();
    UpdateViewMatrix();
    UpdateFrustum();
}
void FEditorViewportClient::ResizeViewport(FRect Top, FRect Bottom, FRect Left, FRect Right)
{
    if (Viewport) {
        Viewport->ResizeViewport(Top, Bottom, Left, Right);
    }
    else {
        UE_LOG(LogLevel::Error, "Viewport is nullptr");
    }
    AspectRatio = GEngineLoop.GetAspectRatio(GEngineLoop.GraphicDevice.SwapChain);
    UpdateProjectionMatrix();
    UpdateViewMatrix();
    UpdateFrustum();
}
bool FEditorViewportClient::IsSelected(POINT point)
{
    float TopLeftX = Viewport->GetViewport().TopLeftX;
    float TopLeftY = Viewport->GetViewport().TopLeftY;
    float Width = Viewport->GetViewport().Width;
    float Height = Viewport->GetViewport().Height;

    if (point.x >= TopLeftX && point.x <= TopLeftX + Width &&
        point.y >= TopLeftY && point.y <= TopLeftY + Height)
    {
        return true;
    }
    return false;
}
D3D11_VIEWPORT& FEditorViewportClient::GetD3DViewport()
{
    return Viewport->GetViewport();
}
void FEditorViewportClient::CameraMoveForward(float _Value)
{
    if (IsPerspective()) {
        FVector curCameraLoc = ViewTransformPerspective.GetLocation();
        curCameraLoc = curCameraLoc + ViewTransformPerspective.GetForwardVector() * GetCameraSpeedScalar() * _Value;
        ViewTransformPerspective.SetLocation(curCameraLoc);
    }
    else
    {
        Pivot.x += _Value * 0.1f;
    }
}

void FEditorViewportClient::CameraMoveRight(float _Value)
{
    if (IsPerspective()) {
        FVector curCameraLoc = ViewTransformPerspective.GetLocation();
        curCameraLoc = curCameraLoc + ViewTransformPerspective.GetRightVector() * GetCameraSpeedScalar() * _Value;
        ViewTransformPerspective.SetLocation(curCameraLoc);
    }
    else
    {
        Pivot.y += _Value * 0.1f;
    }
}

void FEditorViewportClient::CameraMoveUp(float _Value)
{
    if (IsPerspective()) {
        FVector curCameraLoc = ViewTransformPerspective.GetLocation();
        curCameraLoc.z = curCameraLoc.z + GetCameraSpeedScalar() * _Value;
        ViewTransformPerspective.SetLocation(curCameraLoc);
    }
    else {
        Pivot.z += _Value * 0.1f;
    }
}

void FEditorViewportClient::CameraRotateYaw(float _Value)
{
    FVector curCameraRot = ViewTransformPerspective.GetRotation();
    curCameraRot.z += _Value ;
    ViewTransformPerspective.SetRotation(curCameraRot);
}

void FEditorViewportClient::CameraRotatePitch(float _Value)
{
    FVector curCameraRot = ViewTransformPerspective.GetRotation();
    curCameraRot.y += _Value;
    if (curCameraRot.y <= -89.0f)
        curCameraRot.y = -89.0f;
    if (curCameraRot.y >= 89.0f)
        curCameraRot.y = 89.0f;
    ViewTransformPerspective.SetRotation(curCameraRot);
}

void FEditorViewportClient::PivotMoveRight(float _Value)
{
    Pivot = Pivot + ViewTransformOrthographic.GetRightVector() * _Value * -0.05f;
}

void FEditorViewportClient::PivotMoveUp(float _Value)
{
    Pivot = Pivot + ViewTransformOrthographic.GetUpVector() * _Value * 0.05f;
}

void FEditorViewportClient::UpdateViewMatrix()
{
    if (IsPerspective()) {
        View = JungleMath::CreateViewMatrix(ViewTransformPerspective.GetLocation(),
            ViewTransformPerspective.GetLocation() + ViewTransformPerspective.GetForwardVector(),
            FVector{ 0.0f,0.0f,1.0f });
    }
    else 
    {
        UpdateOrthoCameraLoc();
        if (ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoNegativeXY) {
            View = JungleMath::CreateViewMatrix(ViewTransformOrthographic.GetLocation(),
                Pivot, FVector(0.0f, -1.0f, 0.0f));
        }
        else
        {
            View = JungleMath::CreateViewMatrix(ViewTransformOrthographic.GetLocation(),
                Pivot, FVector(0.0f, 0.0f, 1.0f));
        }
    }
    FEngineLoop::Renderer.UpdateViewMatrix(View);
}

void FEditorViewportClient::UpdateProjectionMatrix()
{
    if (IsPerspective()) {
        Projection = JungleMath::CreateProjectionMatrix(
            XMConvertToRadians(ViewFOV),
            GetViewport()->GetViewport().Width/ GetViewport()->GetViewport().Height,
            nearPlane,
            farPlane
        );
    }
    else
    {
        // 스왑체인의 가로세로 비율을 구합니다.
        float aspectRatio = GetViewport()->GetViewport().Width / GetViewport()->GetViewport().Height;

        // 오쏘그래픽 너비는 줌 값과 가로세로 비율에 따라 결정됩니다.
        float orthoWidth = orthoSize * aspectRatio;
        float orthoHeight = orthoSize;

        // 오쏘그래픽 투영 행렬 생성 (nearPlane, farPlane 은 기존 값 사용)
        Projection = JungleMath::CreateOrthoProjectionMatrix(
            orthoWidth,
            orthoHeight,
            nearPlane,
            farPlane
        );
    }
    FEngineLoop::Renderer.UpdateProjectionMatrix(Projection);
    bProjectionUpdated = true;
}

void FEditorViewportClient::UpdateFrustum()
{
    CameraFrustum.CreatePlane(ViewTransformPerspective, ViewFOV, nearPlane, farPlane, AspectRatio);
}

bool FEditorViewportClient::IsOrtho() const
{
    return !IsPerspective();
}

bool FEditorViewportClient::IsPerspective() const
{
    return (GetViewportType() == LVT_Perspective);
}

ELevelViewportType FEditorViewportClient::GetViewportType() const
{
    ELevelViewportType EffectiveViewportType = ViewportType;
    if (EffectiveViewportType == LVT_None)
    {
        EffectiveViewportType = LVT_Perspective;
    }
    //if (bUseControllingActorViewInfo)
    //{
    //    EffectiveViewportType = (ControllingActorViewInfo.ProjectionMode == ECameraProjectionMode::Perspective) ? LVT_Perspective : LVT_OrthoFreelook;
    //}
    return EffectiveViewportType;
}

void FEditorViewportClient::SetViewportType(ELevelViewportType InViewportType)
{
    ViewportType = InViewportType;
    //ApplyViewMode(GetViewMode(), IsPerspective(), EngineShowFlags);

    //// We might have changed to an orthographic viewport; if so, update any viewport links
    //UpdateLinkedOrthoViewports(true);

    //Invalidate();
}

void FEditorViewportClient::UpdateOrthoCameraLoc()
{
    switch (ViewportType)
    {
    case LVT_OrthoXY: // Top
        ViewTransformOrthographic.SetLocation(Pivot + FVector::UpVector*100000.0f);
        ViewTransformOrthographic.SetRotation(FVector(0.0f, 90.0f, -90.0f));
        break;
    case LVT_OrthoXZ: // Front
        ViewTransformOrthographic.SetLocation(Pivot + FVector::ForwardVector*100000.0f);
        ViewTransformOrthographic.SetRotation(FVector(0.0f, 0.0f, 180.0f));
        break;
    case LVT_OrthoYZ: // Left
        ViewTransformOrthographic.SetLocation(Pivot + FVector::RightVector*100000.0f);
        ViewTransformOrthographic.SetRotation(FVector(0.0f, 0.0f, 270.0f));
        break;
    case LVT_Perspective:
        break;
    case LVT_OrthoNegativeXY: // Bottom
        ViewTransformOrthographic.SetLocation(Pivot + FVector::UpVector * -1.0f*100000.0f);
        ViewTransformOrthographic.SetRotation(FVector(0.0f, -90.0f, 90.0f));
        break;
    case LVT_OrthoNegativeXZ: // Back
        ViewTransformOrthographic.SetLocation(Pivot + FVector::ForwardVector * -1.0f*100000.0f);
        ViewTransformOrthographic.SetRotation(FVector(0.0f, 0.0f, 0.0f));
        break;
    case LVT_OrthoNegativeYZ: // Right
        ViewTransformOrthographic.SetLocation(Pivot + FVector::RightVector * -1.0f*100000.0f);
        ViewTransformOrthographic.SetRotation(FVector(0.0f, 0.0f, 90.0f));
        break;
    case LVT_MAX:
        break;
    case LVT_None:
        break;
    default:
        break;
    }
}

void FEditorViewportClient::SetOthoSize(float _Value)
{
    orthoSize += _Value;
    if (orthoSize <= 0.1f)
        orthoSize = 0.1f;
    
}

void FEditorViewportClient::LoadConfig(const TMap<FString, FString>& config)
{
    FString ViewportNum = std::to_string(ViewportIndex);
    CameraSpeedSetting = GetValueFromConfig(config, "CameraSpeedSetting" + ViewportNum, 1);
    CameraSpeedScalar = GetValueFromConfig(config, "CameraSpeedScalar" + ViewportNum, 1.0f);
    GridSize = GetValueFromConfig(config, "GridSize"+ ViewportNum, 10.0f);
    ViewTransformPerspective.ViewLocation.x = GetValueFromConfig(config, "PerspectiveCameraLocX" + ViewportNum, 0.0f);
    ViewTransformPerspective.ViewLocation.y = GetValueFromConfig(config, "PerspectiveCameraLocY" + ViewportNum, 0.0f);
    ViewTransformPerspective.ViewLocation.z = GetValueFromConfig(config, "PerspectiveCameraLocZ" + ViewportNum, 0.0f);
    ViewTransformPerspective.ViewRotation.x = GetValueFromConfig(config, "PerspectiveCameraRotX" + ViewportNum, 0.0f);
    ViewTransformPerspective.ViewRotation.y = GetValueFromConfig(config, "PerspectiveCameraRotY" + ViewportNum, 0.0f);
    ViewTransformPerspective.ViewRotation.z = GetValueFromConfig(config, "PerspectiveCameraRotZ" + ViewportNum, 0.0f);
    ShowFlag = GetValueFromConfig(config, "ShowFlag" + ViewportNum, 31.0f);
    ViewMode = static_cast<EViewModeIndex>(GetValueFromConfig(config, "ViewMode" + ViewportNum, 0));
    ViewportType = static_cast<ELevelViewportType>(GetValueFromConfig(config, "ViewportType" + ViewportNum, 0));
}
void FEditorViewportClient::SaveConfig(TMap<FString, FString>& config)
{
    FString ViewportNum = std::to_string(ViewportIndex);
    config["CameraSpeedSetting"+ ViewportNum] = std::to_string(CameraSpeedSetting);
    config["CameraSpeedScalar"+ ViewportNum] = std::to_string(CameraSpeedScalar);
    config["GridSize"+ ViewportNum] = std::to_string(GridSize);
    config["PerspectiveCameraLocX" + ViewportNum] = std::to_string(ViewTransformPerspective.GetLocation().x);
    config["PerspectiveCameraLocY" + ViewportNum] = std::to_string(ViewTransformPerspective.GetLocation().y);
    config["PerspectiveCameraLocZ" + ViewportNum] = std::to_string(ViewTransformPerspective.GetLocation().z);
    config["PerspectiveCameraRotX" + ViewportNum] = std::to_string(ViewTransformPerspective.GetRotation().x);
    config["PerspectiveCameraRotY" + ViewportNum] = std::to_string(ViewTransformPerspective.GetRotation().y);
    config["PerspectiveCameraRotZ" + ViewportNum] = std::to_string(ViewTransformPerspective.GetRotation().z);
    config["ShowFlag"+ ViewportNum] = std::to_string(ShowFlag);
    config["ViewMode" + ViewportNum] = std::to_string(int32(ViewMode));
    config["ViewportType" + ViewportNum] = std::to_string(int32(ViewportType));
}
TMap<FString, FString> FEditorViewportClient::ReadIniFile(const FString& filePath)
{
    TMap<FString, FString> config;
    std::ifstream file(*filePath);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '[' || line[0] == ';') continue;
        std::istringstream ss(line);
        std::string key, value;
        if (std::getline(ss, key, '=') && std::getline(ss, value)) {
            config[key] = value;
        }
    }
    return config;
}

void FEditorViewportClient::WriteIniFile(const FString& filePath, const TMap<FString, FString>& config)
{
    std::ofstream file(*filePath);
    for (const auto& pair : config) {
        file << *pair.Key << "=" << *pair.Value << "\n";
    }
}

void FEditorViewportClient::SetCameraSpeedScalar(float value)
{
    if (value < 0.198f)
        value = 0.198f;
    else if (value > 176.0f)
        value = 176.0f;
    CameraSpeedScalar = value;
}


FVector FViewportCameraTransform::GetForwardVector()
{
    FVector Forward = FVector(1.f, 0.f, 0.0f);
    Forward = JungleMath::FVectorRotate(Forward, ViewRotation);
    return Forward;
}
FVector FViewportCameraTransform::GetRightVector()
{
    FVector Right = FVector(0.f, 1.f, 0.0f);
	Right = JungleMath::FVectorRotate(Right, ViewRotation);
	return Right;
}

FVector FViewportCameraTransform::GetUpVector()
{
    FVector Up = FVector(0.f, 0.f, 1.0f);
    Up = JungleMath::FVectorRotate(Up, ViewRotation);
    return Up;
}

FViewportCameraTransform::FViewportCameraTransform()
{
}

void Frustum::CreatePlane(FViewportCameraTransform& camera, float fov, float nearZ, float farZ, float aspectRatio)
{
    const FVector point = camera.GetLocation();
    const FVector forward = camera.GetForwardVector();
    const FVector up = camera.GetUpVector();
    const FVector right = camera.GetRightVector();

    const float radFov = fov * PI / 180.0f;
    const float halfFov = radFov * 0.5f;
    const float tanFov = tanf(halfFov);
    const float nearHeight = tanFov * nearZ;
    const float nearWidth = nearHeight * aspectRatio;
    const float farHeight = tanFov * farZ;
    const float farWidth = farHeight * aspectRatio;

    const float deltaZ = farZ - nearZ;
    const float widthDiff = farWidth - nearWidth;
    const float heightDiff = farHeight - nearHeight;
    const FVector fwdDelta = forward * deltaZ;

    // Left plane
    const FVector leftDir = fwdDelta - right * widthDiff;
    planes[0].normal = up.Cross(leftDir).Normalize();
    planes[0].d = -planes[0].normal.Dot(point);

    // Right plane
    const FVector rightDir = fwdDelta + right * widthDiff;
    planes[1].normal = rightDir.Cross(up).Normalize();
    planes[1].d = -planes[1].normal.Dot(point);

    // Top plane
    const FVector upDir = fwdDelta + up * heightDiff;
    planes[2].normal = right.Cross(upDir).Normalize();
    planes[2].d = -planes[2].normal.Dot(point);

    // Bottom plane
    const FVector downDir = fwdDelta - up * heightDiff;
    planes[3].normal = downDir.Cross(right).Normalize();
    planes[3].d = -planes[3].normal.Dot(point);

    // Near plane
    const FVector nearPoint = point + forward * nearZ;
    planes[4].normal = forward;
    planes[4].d = -forward.Dot(nearPoint);

    // Far plane
    const FVector farPoint = point + forward * farZ;
    planes[5].normal = forward * -1.f;
    planes[5].d = forward.Dot(farPoint);
}

void Frustum::CreatePlaneWithMatrix(const FMatrix& ViewProjection)
{
    XMMATRIX ViewProj = ViewProjection.ToXMMATRIX();
    
    // 행렬의 각 행을 SIMD 벡터로 로드합니다.
    XMVECTOR r0 = ViewProj.r[0];
    XMVECTOR r1 = ViewProj.r[1];
    XMVECTOR r2 = ViewProj.r[2];
    XMVECTOR r3 = ViewProj.r[3];

    // SIMD 연산을 사용하여 평면을 계산 및 정규화합니다.
    XMVECTOR simdPlanes[6];
    simdPlanes[0] = XMPlaneNormalize(XMVectorAdd(r3, r0));   // Left plane: row3 + row0
    simdPlanes[1] = XMPlaneNormalize(XMVectorSubtract(r3, r0)); // Right plane: row3 - row0
    simdPlanes[2] = XMPlaneNormalize(XMVectorAdd(r3, r1));   // Top plane: row3 + row1
    simdPlanes[3] = XMPlaneNormalize(XMVectorSubtract(r3, r1)); // Bottom plane: row3 - row1
    simdPlanes[4] = XMPlaneNormalize(XMVectorAdd(r3, r2));   // Near plane: row3 + row2
    simdPlanes[5] = XMPlaneNormalize(XMVectorSubtract(r3, r2)); // Far plane: row3 - row2

    // SIMD 결과를 Plane 배열에 저장합니다.
    for (int i = 0; i < 6; ++i)
    {
        XMFLOAT4 planeF4;
        XMStoreFloat4(&planeF4, simdPlanes[i]);
        planes[i].normal.x = planeF4.x;
        planes[i].normal.y = planeF4.y;
        planes[i].normal.z = planeF4.z;
        planes[i].d = planeF4.w;
    }
}

bool Frustum::Intersects(const FBoundingBox& box) const
{
    for (int i = 0; i < 6; ++i)
    {
        const Plane& Plane = planes[i];
        FVector PositiveVector = box.GetPositiveVertex(Plane.normal);
        float Dist = PositiveVector.Dot(Plane.normal) + Plane.d;
        if (Dist < 0.2f)
        {
            return false;
        }
    }
    return true;
}
