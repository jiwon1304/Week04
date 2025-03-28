#pragma once
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectTypes.h"
class UGizmoBaseComponent;
class UGizmoArrowComponent;
class USceneComponent;
class UPrimitiveComponent;

class AEditorPlayer : public AActor
{
    DECLARE_CLASS(AEditorPlayer, AActor)

    AEditorPlayer();

    virtual void Tick(float DeltaTime) override;

    void SetHWND(HWND InHWnd);
    HWND hWnd;
    void Input(float DeltaTime);
    bool PickGizmo(FVector& rayOrigin);
    void PickActor(const FVector& pickPosition);
    void AddControlMode();
    void AddCoordiMode();

private:
    int RayIntersectsObject(const FVector& pickPosition, USceneComponent* obj, float& hitDistance, int& intersectCount);
    void ScreenToViewSpace(int screenX, int screenY, const FMatrix& viewMatrix, const FMatrix& projectionMatrix, FVector& rayOrigin);
    void PickedObjControl(float DeltaTime);
    void ControlRotation(USceneComponent* pObj, UGizmoBaseComponent* Gizmo, int32 deltaX, int32 deltaY);
    void ControlTranslation(USceneComponent* pObj, UGizmoBaseComponent* Gizmo, int32 deltaX, int32 deltaY);
    void ControlScale(USceneComponent* pObj, UGizmoBaseComponent* Gizmo, int32 deltaX, int32 deltaY);
    bool bLeftMouseDown = false;
    bool bRightMouseDown = false;
    bool bSpaceDown = false;

    POINT m_LastMousePos;
    ControlMode cMode = CM_TRANSLATION;
    CoordiMode cdMode = CDM_WORLD;

    inline static int TotalPickCount = 0;
    inline static double accumulatedPickingTime = 0;
    double curPickingTime = 0;

public:
    void SetMode(ControlMode _Mode) { cMode = _Mode; }
    ControlMode GetControlMode() const { return cMode; }
    CoordiMode GetCoordiMode() const { return cdMode; }
    uint32 GetTotalPickCount() const { return TotalPickCount; }
    double GetCurPickingTime() const { return curPickingTime; }
    double GetAccumulatedPickingTime() const { return accumulatedPickingTime; }
};
