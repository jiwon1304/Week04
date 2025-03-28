#include "Engine/Source/Runtime/Engine/World.h"

#include "Actors/Player.h"
#include "BaseGizmos/TransformGizmo.h"
#include "Camera/CameraComponent.h"
#include "LevelEditor/SLevelEditor.h"
#include "Engine/FLoaderOBJ.h"
#include "Classes/Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SkySphereComponent.h"


void UWorld::Initialize()
{
    // TODO: Load Scene
    CreateBaseObject();

    FManagerOBJ::CreateStaticMesh("Assets/apple_mid.obj");

    FManagerOBJ::CreateStaticMesh("Assets/bitten_apple_mid.obj");

#ifdef _DEBUG
    AActor* SpawnedActor = SpawnActor<AActor>();
    UStaticMeshComponent* AppleMesh = SpawnedActor->AddComponent<UStaticMeshComponent>();
    AppleMesh->SetStaticMesh(FManagerOBJ::GetStaticMesh(L"apple_mid.obj"));
#else
    for (int i = 0; i < 50; i++)
    {
        for (int j = 0; j < 50; j++)
        {
            for (int k = 0; k < 20; k++)
            {
                AActor* SpawnedActor = SpawnActor<AActor>();
                UStaticMeshComponent* AppleMesh = SpawnedActor->AddComponent<UStaticMeshComponent>();
                AppleMesh->SetStaticMesh(FManagerOBJ::GetStaticMesh(L"apple_mid.obj"));
                SpawnedActor->SetActorLocation(FVector(i, j, k));
            }
        }
    }
#endif
}

void UWorld::CreateBaseObject()
{
    if (EditorPlayer == nullptr)
    {
        EditorPlayer = FObjectFactory::ConstructObject<AEditorPlayer>();;
    }

    if (camera == nullptr)
    {
        camera = FObjectFactory::ConstructObject<UCameraComponent>();
        camera->SetLocation(FVector(8.0f, 8.0f, 8.f));
        camera->SetRotation(FVector(0.0f, 45.0f, -135.0f));
    }

    if (LocalGizmo == nullptr)
    {
        LocalGizmo = FObjectFactory::ConstructObject<UTransformGizmo>();
    }
}

void UWorld::ReleaseBaseObject()
{
    if (LocalGizmo)
    {
        delete LocalGizmo;
        LocalGizmo = nullptr;
    }

    /* W04
    if (worldGizmo)
    {
        delete worldGizmo;
        worldGizmo = nullptr;
    }
    */

    if (camera)
    {
        delete camera;
        camera = nullptr;
    }

    if (EditorPlayer)
    {
        delete EditorPlayer;
        EditorPlayer = nullptr;
    }

}

void UWorld::Tick(float DeltaTime)
{
	//camera->TickComponent(DeltaTime); // W04
	EditorPlayer->Tick(DeltaTime); // TODO: W04 - 최적화 하기
	// LocalGizmo->Tick(DeltaTime); // TODO: W04 - 기즈모 조작 필요하면 주석 제거

    /* W04
    // SpawnActor()에 의해 Actor가 생성된 경우, 여기서 BeginPlay 호출
    for (AActor* Actor : PendingBeginPlayActors)
    {
        Actor->BeginPlay();
    }
    PendingBeginPlayActors.Empty();

    // 매 틱마다 Actor->Tick(...) 호출
	for (AActor* Actor : ActorsArray)
	{
	    Actor->Tick(DeltaTime);
	}
	*/
}

void UWorld::SetPickedActor(AActor* InActor)
{
    SelectedActor = InActor;

    // W04 - LocalGizmo의 Tick에서 하던걸 선택시 한번만 하게 변경. 기즈모 조작을 하지 않는다고 가정했기 때문. 
    LocalGizmo->SetActorLocation(SelectedActor->GetActorLocation());
    /*
    if (GetWorld()->GetEditorPlayer()->GetCoordiMode() == CoordiMode::CDM_LOCAL)
    {
        // TODO: 임시로 RootComponent의 정보로 사용
        SetActorRotation(PickedActor->GetActorRotation());
    }
    else if (GetWorld()->GetEditorPlayer()->GetCoordiMode() == CoordiMode::CDM_WORLD)
        SetActorRotation(FVector(0.0f, 0.0f, 0.0f));
    */
}


void UWorld::Release()
{
	for (AActor* Actor : ActorsArray)
	{
		Actor->EndPlay(EEndPlayReason::WorldTransition);
        TSet<UActorComponent*> Components = Actor->GetComponents();
	    for (UActorComponent* Component : Components)
	    {
	        GUObjectArray.MarkRemoveObject(Component);
	    }
	    GUObjectArray.MarkRemoveObject(Actor);
	}
    ActorsArray.Empty();

	pickingGizmo = nullptr;
	ReleaseBaseObject();

    GUObjectArray.ProcessPendingDestroyObjects();
}

bool UWorld::DestroyActor(AActor* ThisActor)
{
    if (ThisActor->GetWorld() == nullptr)
    {
        return false;
    }

    if (ThisActor->IsActorBeingDestroyed())
    {
        return true;
    }

    // 액터의 Destroyed 호출
    ThisActor->Destroyed();

    if (ThisActor->GetOwner())
    {
        ThisActor->SetOwner(nullptr);
    }

    TSet<UActorComponent*> Components = ThisActor->GetComponents();
    for (UActorComponent* Component : Components)
    {
        Component->DestroyComponent();
    }

    // World에서 제거
    ActorsArray.Remove(ThisActor);

    // 제거 대기열에 추가
    GUObjectArray.MarkRemoveObject(ThisActor);
    return true;
}

void UWorld::SetPickingGizmo(UObject* Object)
{
	pickingGizmo = Cast<USceneComponent>(Object);
}