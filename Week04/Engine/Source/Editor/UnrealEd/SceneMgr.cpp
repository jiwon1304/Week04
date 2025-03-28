#include "UnrealEd/SceneMgr.h"
#include "JSON/json.hpp"
#include "UObject/Object.h"
#include "Components/SphereComp.h"
#include "Components/CubeComp.h"
#include "BaseGizmos/GizmoArrowComponent.h"
#include "UObject/ObjectFactory.h"
#include <fstream>
#include "Components/UBillboardComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkySphereComponent.h"
#include "Camera/CameraComponent.h"
#include "UObject/Casts.h"
#include "Engine/FLoaderOBJ.h"

using json = nlohmann::json;

SceneData FSceneMgr::ParseSceneData(const FString& jsonStr)
{
    SceneData sceneData;

    try {
        json j = json::parse(*jsonStr);

        // 버전과 NextUUID 읽기
        //sceneData.Version = j["Version"].get<int>();
        sceneData.NextUUID = j["NextUUID"].get<int>();

        // Primitives 처리 (C++14 스타일)
        auto primitives = j["Primitives"];
        for (auto it = primitives.begin(); it != primitives.end(); ++it) {
            int id = std::stoi(it.key());  // Key는 문자열, 숫자로 변환
            const json& value = it.value();
            UObject* obj = nullptr;
            if (value.contains("Type"))
            {
                const FString TypeName = value["Type"].get<std::string>();
                // 현재 내준 default.scene에는  "StaticMeshComp"로 저장되어있음
                // StaticClass의 GetName과 다름 
                // TODO : 바꾸기
                if (TypeName == FString("StaticMeshComp"))
                {
                    obj = FObjectFactory::ConstructObject<UStaticMeshComponent>();
                }
                 else if (TypeName == USphereComp::StaticClass()->GetName())
                {
                    obj = FObjectFactory::ConstructObject<USphereComp>();
                }
                else if (TypeName == UCubeComp::StaticClass()->GetName())
                {
                    obj = FObjectFactory::ConstructObject<UCubeComp>();
                }
                else if (TypeName == UGizmoArrowComponent::StaticClass()->GetName())
                {
                    obj = FObjectFactory::ConstructObject<UGizmoArrowComponent>();
                }
                else if (TypeName == UBillboardComponent::StaticClass()->GetName())
                {
                    obj = FObjectFactory::ConstructObject<UBillboardComponent>();
                }
                else if (TypeName == ULightComponentBase::StaticClass()->GetName())
                {
                    obj = FObjectFactory::ConstructObject<ULightComponentBase>();
                }
                else if (TypeName == USkySphereComponent::StaticClass()->GetName())
                {
                    obj = FObjectFactory::ConstructObject<USkySphereComponent>();
                    USkySphereComponent* skySphere = static_cast<USkySphereComponent*>(obj);
                }
            }

            USceneComponent* sceneComp = static_cast<USceneComponent*>(obj);
            //Todo : 여기다가 Obj Maeh저장후 일기
            if (value.contains("ObjStaticMeshAsset"))
            {
                FString MeshPath = value["ObjStaticMeshAsset"].get<std::string>();
                FManagerOBJ::CreateStaticMesh(MeshPath);

                int32 LastDirectoryIndex = MeshPath.Find(FString("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, -1);
                FString MeshFileName = MeshPath.SubStr(LastDirectoryIndex + 1);

                (Cast<UStaticMeshComponent>(obj))->SetStaticMesh(FManagerOBJ::GetStaticMesh(MeshFileName.ToWideString()));
            }
            if (value.contains("Location")) sceneComp->SetLocation(FVector(value["Location"].get<std::vector<float>>()[0],
                value["Location"].get<std::vector<float>>()[1],
                value["Location"].get<std::vector<float>>()[2]));
            if (value.contains("Rotation")) sceneComp->SetRotation(FVector(value["Rotation"].get<std::vector<float>>()[0],
                value["Rotation"].get<std::vector<float>>()[1],
                value["Rotation"].get<std::vector<float>>()[2]));
            if (value.contains("Scale")) sceneComp->SetScale(FVector(value["Scale"].get<std::vector<float>>()[0],
                value["Scale"].get<std::vector<float>>()[1],
                value["Scale"].get<std::vector<float>>()[2]));
            if (value.contains("Type")) {
                UPrimitiveComponent* primitiveComp = Cast<UPrimitiveComponent>(sceneComp);
                if (primitiveComp) {
                    primitiveComp->SetType(value["Type"].get<std::string>());
                }
                else {
                    std::string name = value["Type"].get<std::string>();
                    sceneComp->NamePrivate = name.c_str();
                }
            }
            sceneData.Primitives[id] = sceneComp;
        }

        // Cameras 처리
        if (j.contains("Cameras")) {
            // 아직 추가 안함
            assert(0);
            //// 여러 개의 카메라 처리
            //auto cameras = j["Cameras"];
            //for (auto it = cameras.begin(); it != cameras.end(); ++it) {
            //    int cameraID = std::stoi(it.key());
            //    const json& cameraValue = it.value();

            //    FCameraData cameraData;

            //    if (cameraValue.contains("Type"))
            //        cameraData.Type = FString(cameraValue["Type"].get<std::string>().c_str());

            //    if (cameraValue.contains("FOV"))
            //        cameraData.FOV = cameraValue["FOV"].get<std::vector<float>>()[0];

            //    if (cameraValue.contains("NearClip"))
            //        cameraData.NearClip = cameraValue["NearClip"].get<std::vector<float>>()[0];

            //    if (cameraValue.contains("FarClip"))
            //        cameraData.FarClip = cameraValue["FarClip"].get<std::vector<float>>()[0];

            //    if (cameraValue.contains("Location")) {
            //        cameraData.Location = FVector(
            //            cameraValue["Location"].get<std::vector<float>>()[0],
            //            cameraValue["Location"].get<std::vector<float>>()[1],
            //            cameraValue["Location"].get<std::vector<float>>()[2]);
            //    }

            //    if (cameraValue.contains("Rotation")) {
            //        cameraData.Rotation = FVector(
            //            cameraValue["Rotation"].get<std::vector<float>>()[0],
            //            cameraValue["Rotation"].get<std::vector<float>>()[1],
            //            cameraValue["Rotation"].get<std::vector<float>>()[2]);
            //    }

            //    sceneData.Cameras.Add(cameraID, cameraData);
            //}
        }
        else if (j.contains("PerspectiveCamera")) {
            // 단일 PerspectiveCamera 처리
            const json& cameraValue = j["PerspectiveCamera"];
            UCameraComponent* cam = FObjectFactory::ConstructObject<UCameraComponent>();
            cam->InitializeComponent();

            // perspective라고 가정하고 진행.
            cam->SetFOV(cameraValue["FOV"].get<std::vector<float>>()[0]);
            cam->SetNearClip(cameraValue["NearClip"].get<std::vector<float>>()[0]);
            cam->SetFarClip(cameraValue["FarClip"].get<std::vector<float>>()[0]);
            cam->SetLocation(FVector(
                cameraValue["Location"].get<std::vector<float>>()[0],
                cameraValue["Location"].get<std::vector<float>>()[1],
                cameraValue["Location"].get<std::vector<float>>()[2])
            );
            cam->SetRotation(FVector(
                cameraValue["Rotation"].get<std::vector<float>>()[0],
                cameraValue["Rotation"].get<std::vector<float>>()[1],
                cameraValue["Rotation"].get<std::vector<float>>()[2])
            );

            sceneData.Cameras.Add(0, cam); // 단일 카메라일 경우 ID를 0으로 설정
        }
    }
    catch (const std::exception& e) {
        FString errorMessage = "Error parsing JSON: ";
        errorMessage += e.what();

        UE_LOG(LogLevel::Error, "No Json file");
    }

    return sceneData;
}

FString FSceneMgr::LoadSceneFromFile(const FString& filename)
{
    std::ifstream inFile(*filename);
    json j;
    try {
        inFile >> j; // JSON 파일 읽기
    }
    catch (const std::exception& e) {
        UE_LOG(LogLevel::Error, "Error parsing JSON: %s", e.what());
        return FString();
    }

    inFile.close();

    return j.dump(4);
}

std::string FSceneMgr::SerializeSceneData(const SceneData& sceneData)
{
    json j;

    // Version과 NextUUID 저장
    //j["Version"] = sceneData.Version;
    j["NextUUID"] = sceneData.NextUUID;

    // Primitives 처리 (C++17 스타일)
    for (const auto& [Id, Obj] : sceneData.Primitives)
    {
        USceneComponent* primitive = static_cast<USceneComponent*>(Obj);
        std::vector<float> Location = { primitive->GetWorldLocation().x,primitive->GetWorldLocation().y,primitive->GetWorldLocation().z };
        std::vector<float> Rotation = { primitive->GetWorldRotation().x,primitive->GetWorldRotation().y,primitive->GetWorldRotation().z };
        std::vector<float> Scale = { primitive->GetWorldScale().x,primitive->GetWorldScale().y,primitive->GetWorldScale().z };

        std::string primitiveName = *primitive->GetName();
        size_t pos = primitiveName.rfind('_');
        if (pos != INDEX_NONE) {
            primitiveName = primitiveName.substr(0, pos);
        }
        j["Primitives"][std::to_string(Id)] = {
            {"Location", Location},
            {"Rotation", Rotation},
            {"Scale", Scale},
            {"Type",primitiveName}
        };
    }

    for (const auto& [id, camera] : sceneData.Cameras)
    {
        UCameraComponent* cameraComponent = static_cast<UCameraComponent*>(camera);
        TArray<float> Location = { cameraComponent->GetWorldLocation().x, cameraComponent->GetWorldLocation().y, cameraComponent->GetWorldLocation().z };
        TArray<float> Rotation = { 0.0f, cameraComponent->GetWorldRotation().y, cameraComponent->GetWorldRotation().z };
        float FOV = cameraComponent->GetFOV();
        float nearClip = cameraComponent->GetNearClip();
        float farClip = cameraComponent->GetFarClip();
    
        //
        j["PerspectiveCamera"][std::to_string(id)] = {
            {"Location", Location},
            {"Rotation", Rotation},
            {"FOV", FOV},
            {"NearClip", nearClip},
            {"FarClip", farClip}
        };
    }


    return j.dump(4); // 4는 들여쓰기 수준
}

bool FSceneMgr::SaveSceneToFile(const FString& filename, const SceneData& sceneData)
{
    std::ofstream outFile(*filename);
    if (!outFile) {
        FString errorMessage = "Failed to open file for writing: ";
        MessageBoxA(NULL, *errorMessage, "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    std::string jsonData = SerializeSceneData(sceneData);
    outFile << jsonData;
    outFile.close();

    return true;
}

