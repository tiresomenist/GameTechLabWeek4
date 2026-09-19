#include "pch.h"
#include "Scene.h"
#include "SceneValidation.h"
#include <memory>
#include <stdexcept>
// TEMP(UI test): Gizmo implementation is currently excluded from the build.
// #include "Engine/Gizmo/UGizmo.h"

#include "Engine/Component/CameraComponent.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/ActorComponent.h"
#include "Engine/Component/SceneComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Component/Primitive/FlipbookComponent.h"
#include "Engine/Component/WidgetComponent.h"
#include "Engine/Component/Primitive/PrimitiveComponent.h"

#include "Core/Serialization/Archive.h"
#include "Engine/Object/ClassRegistry.h"
#include "Engine/Log.h"

FSceneType* UScene::GetStaticSceneType()
{
    static FSceneType Type
    {
        .Name = "Scene",
        .SceneConstructor = []() -> UScene* { return new UScene(); },
    };

    return &Type;
}

// UScene의 BeginPlay, Tick, EndPlay는 모든 Scene에 대한 공통 로직이 필요하면 작성
// But 아직 그런 용도가 없음 언젠가 생기면 쓰는걸로...
void UScene::BeginPlay()
{
    for (AActor* Actor : Actors)
    {
        Actor->BeginPlay();
    }
}

void UScene::Tick(float DeltaTime)
{
    for (AActor* Actor : Actors)
    {
        Actor->Tick(DeltaTime);
    }
}

void UScene::EndPlay()
{
    for (AActor* Actor : Actors)
    {
        Actor->EndPlay();
    }
}

void UScene::CreateMainCamera()
{
    AActor* CameraActor = SpawnActor<AActor*>(AActor::GetClass());
    MainCamera = static_cast<UCameraComponent*>(CameraActor->CreateComponent(UCameraComponent::GetClass()));
}

void UScene::Serialize(FArchive& Archive) {
    const bool bLoading = Archive.IsLoading();
    TArray<UActorComponent*> SavedComponents;

    // 저장할 컴포넌트를 모아 맵의 원소 개수를 먼저 확정한다.
    if (!bLoading)
    {
        for (AActor* Actor : Actors)
        {
            for (UActorComponent* Component : Actor->GetComponents())
            {
                // 지금기준 UUID용 UUID는 저장하지 않음.
                if (!Component->IsA(UWidgetComponent::GetClass()))
                    SavedComponents.Add(Component);
            }
        }
    }
    uint32 Count = static_cast<uint32>(SavedComponents.Num());
    if (!Archive.BeginMap("Primitives", Count))
        throw std::runtime_error("Missing scene primitives.");

    // 키값:UUID
    for (uint32 Index = 0; Index < Count; ++Index)
    {
        UActorComponent* Component = bLoading ? nullptr : SavedComponents[Index];
        FString Key = bLoading ? FString{} : std::to_string(Component->GetUUID());
        Archive.BeginMapEntry(Index, Key);

        FString TypeText = bLoading ? FString{} : Component->GetInstanceClass()->Name.ToString();
        Archive.Field("Type", TypeText);

        if (!bLoading)
        {
            // 객체 생성에 필요한 식별 정보와 소유 Actor 정보를 기록한다.
            AActor* Actor = Component->GetOwner();
            uint32 UUID = Component->GetUUID();
            uint32 ActorUUID = Actor->GetUUID();
            FString ActorName = Actor->GetName().ToString();

            Archive.Field("UUID", UUID);
            Archive.OptionalField("ActorUUID", ActorUUID);
            Archive.OptionalField("ActorName", ActorName);
            Component->Serialize(Archive);
        }
        else
        {
            // 저장된 타입을 실제 생성할 컴포넌트 클래스로 해석한다.
            const FName TypeName(TypeText);
            const FResolvedSceneType Resolved = ResolveSceneType(TypeName);
            if (!Resolved.IsValid())
                throw std::runtime_error("Unsupported scene type: " + TypeText);

            // 구형 파일의 UUID 위젯은 직접 복원하지 않고 마지막에 다시 생성한다.
            if (Resolved.Kind == ESceneTypeKind::SkipRuntimeWidget)
            {
                Archive.EndMapEntry();
                continue;
            }

            // 기존 로더와 동일하게 맵의 키를 실제 컴포넌트 UUID로 사용한다.
            const uint32 UUID = ParseSceneUUID(Key);
            uint32 ActorUUID = 0;
            FString ActorName;
            const bool bHasActorUUID = Archive.OptionalField("ActorUUID", ActorUUID);
            const bool bHasActorName = Archive.OptionalField("ActorName", ActorName);
            AActor* Actor = nullptr;

            // 같은 ActorUUID를 가진 컴포넌트는 하나의 Actor에 모아서 복원한다.
            if (bHasActorUUID)
            {
                for (AActor* ExistingActor : Actors)
                {
                    if (ExistingActor->GetUUID() == ActorUUID)
                    {
                        Actor = ExistingActor;
                        break;
                    }
                }
                if (Actor == nullptr)
                    Actor = SpawnActor<AActor*>(AActor::GetClass(), ActorUUID);
            }
            else
            {
                // Actor 정보가 없는 구형 씬은 컴포넌트마다 Actor를 생성한다.
                Actor = SpawnActor<AActor*>(AActor::GetClass());
            }

            if (bHasActorName)
                Actor->SetName(FName(ActorName));

            // 객체 생성 후 동일한 Archive 위치에서 컴포넌트 속성을 복원한다.
            Component = Actor->CreateComponent(Resolved.ClassType, UUID);
            if (Component == nullptr)
                throw std::runtime_error("Failed to create scene component: " + TypeText);
            Component->Serialize(Archive);

            // Cube 등 구형 타입 이름은 StaticMeshComponent의 MeshKey로 복원한다.
            if (Resolved.Kind == ESceneTypeKind::LegacyStaticMesh)
                static_cast<UStaticMeshComponent*>(Component)->SetStaticMesh(TypeName);

            if (MainCamera == nullptr && Component->IsA(UCameraComponent::GetClass()))
                MainCamera = static_cast<UCameraComponent*>(Component);

            UE_LOG("[Object Restored] UUID:{} Name:{} ActorName:{}",
                Component->GetUUID(), Component->GetName().ToString(), Actor->GetName().ToString());
        }

        Archive.EndMapEntry();
    }
    Archive.EndMap();

    // 모든 컴포넌트가 복원된 뒤 런타임 UUID 표시를 구성한다.
    if (bLoading) EnsureUUIDWidgets();
}


void UScene::EnsureUUIDWidgets()
{
	for (AActor* Actor : Actors)
	{
		USceneComponent* Root = Actor->GetRootComponent();
		if (!Root || !Root->IsA(UPrimitiveComponent::GetClass())) continue;

		bool bHasWidget = false;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component->IsA(UWidgetComponent::GetClass()))
			{
				bHasWidget = true;
				break;
			}
		}

		if (!bHasWidget)
		{
			Actor->CreateComponent(UWidgetComponent::GetClass());
		}
	}
}

void UScene::Destroy(UObject* Object)
{
    if (Object == nullptr)
    {
        return;
    }

    if (Object->IsA(AActor::GetClass()))
    {
        DestroyActor(static_cast<AActor*>(Object));
        return;
    }

    if (Object->IsA(UActorComponent::GetClass()))
    {
        UActorComponent* Component = static_cast<UActorComponent*>(Object);
        DestroyActor(Component->GetOwner());
    }
}

void UScene::DestroyActor(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    bool bExistsInScene = false;
    for (AActor* ExistingActor : Actors)
    {
        if (ExistingActor == Actor)
        {
            bExistsInScene = true;
            break;
        }
    }
    if (!bExistsInScene)
    {
        return;
    }

    // 복사본을 사용한다. 자식 삭제 과정에서 부모의 ChildActors는 함께 갱신된다.
    const TArray<AActor*> Children = Actor->GetChildActors();
    for (AActor* Child : Children)
    {
        DestroyActor(Child);
    }

    for (int32 Index = 0; Index < Actors.Num(); ++Index)
    {
        if (Actors[Index] == Actor)
        {
            if (MainCamera != nullptr && MainCamera->GetOwner() == Actor)
            {
                MainCamera = nullptr;
            }

            Actor->EndPlay();
            delete Actor;
            Actors.RemoveAt(Index);
            break;
        }
    }
}

UScene::~UScene()
{
    for (AActor* Actor : Actors)
    {
        delete Actor;
    }

    Actors.Empty();
}
