#pragma once
#include "Define.h"
#include "Container/Set.h"
#include "UObject/ObjectFactory.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Classes/Components/PrimitiveComponent.h"
#include "Engine/Classes/Components/StaticMeshComponent.h"
#include "Editor/UnrealEd/EditorViewportClient.h"
#include "CoreUObject/UObject/Casts.h"

class FObjectFactory;
class AActor;
class UObject;
class UGizmoArrowComponent;
class UCameraComponent;
class AEditorPlayer;
class USceneComponent;
class UTransformGizmo;
class SceneData;

struct FOctreeNode;

class UWorld : public UObject
{
    DECLARE_CLASS(UWorld, UObject)

public:
    UWorld() = default;

    void Initialize(HWND hWnd);
    void CreateBaseObject(HWND hWnd);
    void ReleaseBaseObject();
    /**
     * @return 렌더러를 업데이트 해야하는 경우 true
     */
    bool Tick(float DeltaTime);
    void Release();
    void LoadSceneData(SceneData Scene, std::shared_ptr<FEditorViewportClient> ViewportClient);

    /**
     * World에 Actor를 Spawn합니다.
     * @tparam T AActor를 상속받은 클래스
     * @return Spawn된 Actor의 포인터
     */
    template <typename T>
        requires std::derived_from<T, AActor>
    T* SpawnActor();

    /** World에 존재하는 Actor를 제거합니다. */
    bool DestroyActor(AActor* ThisActor);

private:
    const FString defaultMapName = "Default";

    /** World에서 관리되는 모든 Actor의 목록 */
    TSet<AActor*> ActorsArray;

    /** Actor가 Spawn되었고, 아직 BeginPlay가 호출되지 않은 Actor들 */
    TArray<AActor*> PendingBeginPlayActors;

    AActor* SelectedActor = nullptr;

    USceneComponent* pickingGizmo = nullptr;
    UCameraComponent* camera = nullptr;
    AEditorPlayer* EditorPlayer = nullptr;

    std::unique_ptr<FOctreeNode> RootOctree = nullptr;

public:
    // UObject* worldGizmo = nullptr; // W04

    const TSet<AActor*>& GetActors() const { return ActorsArray; }

    UTransformGizmo* LocalGizmo = nullptr;
    UCameraComponent* GetCamera() const { return camera; }
    AEditorPlayer* GetEditorPlayer() const { return EditorPlayer; }


    // EditorManager 같은데로 보내기
    AActor* GetSelectedActor() const { return SelectedActor; }
    void SetPickedActor(AActor* InActor);

    // UObject* GetWorldGizmo() const { return worldGizmo; } // W04
    USceneComponent* GetPickingGizmo() const { return pickingGizmo; }
    void SetPickingGizmo(UObject* Object);

    FOctreeNode* GetOctree() { return RootOctree.get(); }
};


template <typename T>
    requires std::derived_from<T, AActor>
T* UWorld::SpawnActor()
{
    T* Actor = FObjectFactory::ConstructObject<T>();
    // TODO: 일단 AddComponent에서 Component마다 초기화
    // 추후에 RegisterComponent() 만들어지면 주석 해제
    // Actor->InitializeComponents();
    ActorsArray.Add(Actor);
    PendingBeginPlayActors.Add(Actor);
    return Actor;
}
