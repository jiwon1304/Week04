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

struct FOctreeNode {
    FBoundingBox bound; //현재 노드의 공간 범위
    TArray<UPrimitiveComponent*> components; //현재 노드에 포함된 물체 리스트
    std::unique_ptr<FOctreeNode> children[8]; //자식 옥트리
    bool bIsLeaf;

    FOctreeNode(FVector min, FVector max) : bound(min, max) {}

    void SubDivide() {
        FVector center = bound.GetCenter();
        FVector extent = bound.GetExtent();
        FVector half = extent * 0.5f;

        for (int i = 0; i < 8; ++i) {
            FVector offset(
                (i & 1) ? half.x : -half.x,
                (i & 2) ? half.y : -half.y,
                (i & 4) ? half.z : -half.z
            );
            FVector childCenter = center + offset;
            FVector min = childCenter - half;
            FVector max = childCenter + half;

            children[i] = std::make_unique<FOctreeNode>(min, max);
        }
        bIsLeaf = false;
    }

    bool Insert(UPrimitiveComponent* comp, int depth = 0) {
        if (!bound.IntersectsAABB(comp->GetBoundingBox())) {
            return false;
        }
        if (bIsLeaf) {
            components.Add(comp);
            if (components.Num() > 32 && depth < 12) {
                SubDivide();
                TArray<UPrimitiveComponent*> temp = components;
                components.Empty();

                for (UPrimitiveComponent* component : temp) {
                    bool bInserted = false;
                    for (int i = 0; i < 8; ++i) {
                        if (children[i]->Insert(component, depth + 1)) {
                            bInserted = true;
                            break;
                        }
                    }
                    if (!bInserted)
                        components.Add(component);
                }
            }
            return true;
        }
        else {
            for (int i = 0; i < 8; ++i) {
                if (children[i]->Insert(comp, depth + 1))
                    return true;
            }
            components.Add(comp);
            return true;
        }
    }

    void FrustumCull(Frustum& frustum, TArray<UPrimitiveComponent*>& outComponents) {
        if (!frustum.Intersects(bound)) {
            return;
        }
        if (components.Num() > 0) {
            int a = 0;
            int b = 0;
        }
        if (bIsLeaf) {
            for (UPrimitiveComponent* comp : components) {
                if (UStaticMeshComponent* staticMesh = Cast<UStaticMeshComponent>(comp)) {
                    FBoundingBox bounding = staticMesh->GetBoundingBox();
                }
                if (frustum.Intersects(comp->GetBoundingBox())) {
                    outComponents.Add(comp);
                }
            }
        }
        else {
            for (int i = 0; i < 8; ++i) {
                if (children[i]) {
                    children[i]->FrustumCull(frustum, outComponents);
                }
            }
        }
    }

    uint32 CountAllComponents() const {
        uint32 count = components.Num();
        if (!bIsLeaf) {
            for (int i = 0; i < 8; ++i) {
                if (children[i]) {
                    count += children[i]->CountAllComponents();
                }
            }
        }
        return count;
    }
};

class UWorld : public UObject
{
    DECLARE_CLASS(UWorld, UObject)

public:
    UWorld() = default;

    void Initialize(HWND hWnd);
    void CreateBaseObject(HWND hWnd);
    void ReleaseBaseObject();
    void Tick(float DeltaTime);
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
