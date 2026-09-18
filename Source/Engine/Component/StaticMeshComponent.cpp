#include "pch.h"
#include "StaticMeshComponent.h"
#include "Engine/Object/Archive.h"
#include "Engine/Resource/TextureResource.h"
#include "Engine/Resource/ResourceManager.h"

void UStaticMeshComponent::SetStaticMesh(const FName& InMeshKey)
{
    MeshKey = InMeshKey;
}

FMeshResource* UStaticMeshComponent::GetMeshResource() const
{
    return GResourceManager::GetInstance()->GetPrimitive(MeshKey);
}

void UStaticMeshComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
    Archive.SetString("MeshKey", MeshKey.IsNone() ? FString{} : MeshKey.ToString());
    Archive.SetString("MaterialPath", MaterialPath);
}

void UStaticMeshComponent::Deserialize(FArchive& Archive)
{
    Super::Deserialize(Archive);
    const FName LoadedMeshKey = Archive.Contains("MeshKey")
        ? FName(Archive.GetString("MeshKey"))
        : FName{};
    SetStaticMesh(LoadedMeshKey);
    if (Archive.Contains("MaterialPath"))
        SetMaterial(Archive.GetString("MaterialPath"));
}
