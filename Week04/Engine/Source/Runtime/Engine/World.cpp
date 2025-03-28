#include "Engine/Source/Runtime/Engine/World.h"

#include "Actors/Player.h"
#include "BaseGizmos/TransformGizmo.h"
#include "Camera/CameraComponent.h"
#include "LevelEditor/SLevelEditor.h"
#include "Engine/FLoaderOBJ.h"
#include "Engine/Source/Editor/UnrealEd/SceneMgr.h"
#include "Classes/Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SkySphereComponent.h"


void UWorld::Initialize()
{
    // TODO: Load Scene
    CreateBaseObject();
    //SpawnObject(OBJ_CUBE);
    //FManagerOBJ::CreateStaticMesh("Assets/Dodge/Dodge.obj");

    //FManagerOBJ::CreateStaticMesh("Assets/SkySphere.obj");

    //USkySphereComponent* skySphere = SpawnedActor->AddComponent<USkySphereComponent>();
    //skySphere->SetStaticMesh(FManagerOBJ::GetStaticMesh(L"SkySphere.obj"));
    //skySphere->GetStaticMesh()->GetMaterials()[0]->Material->SetDiffuse(FVector((float)32/255, (float)171/255, (float)191/255));

    /*

    for (int i = 0; i < 100; i++)
    {
        for (int j = 0; j < 100; j++)
        {
            for (int k = 0; k < 5; k++)
            {
                AActor* SpawnedActor = SpawnActor<AActor>();
                UStaticMeshComponent* AppleMesh = SpawnedActor->AddComponent<UStaticMeshComponent>();
                AppleMesh->SetStaticMesh(FManagerOBJ::GetStaticMesh(L"apple_mid.obj"));
                SpawnedActor->SetActorLocation(FVector(i, j, k));
            }
        }
    }
    */

    //FManagerOBJ::CreateStaticMesh("Assets/JungleApples/apple_mid.obj");
    //AActor* SpawnedActor = SpawnActor<AActor>();
    //UStaticMeshComponent* Mesh = SpawnedActor->AddComponent<UStaticMeshComponent>();

    //Mesh->SetupAttachment(SpawnedActor->GetRootComponent());
    //SpawnedActor->AddExternalComponent(Mesh);
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

    if (worldGizmo)
    {
        delete worldGizmo;
        worldGizmo = nullptr;
    }

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
	camera->TickComponent(DeltaTime);
	EditorPlayer->Tick(DeltaTime);
	LocalGizmo->Tick(DeltaTime);

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

void UWorld::LoadSceneData(SceneData Scene)  
{  
   // 현재는 UUID까지 로드하지는 않음
   // camera  
   this->camera = Cast<UCameraComponent>(Scene.Cameras.begin()->Value);  
   
   // primitives  
   for (auto iter = Scene.Primitives.begin(); iter != Scene.Primitives.end(); ++iter)  
   {  
       if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(iter->Value))
       {
           AActor* SpawnedActor = SpawnActor<AActor>();
           
           Mesh->SetupAttachment(SpawnedActor->GetRootComponent());
           SpawnedActor->AddExternalComponent(Mesh);
       }
   }
   CreateBaseObject();

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