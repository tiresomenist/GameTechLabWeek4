#include "pch.h"
#include "StaticMeshComponent.h"
#include "Engine/Object/Archive.h"
#include "Engine/Resource/TextureResource.h"
#include "Engine/Resource/ResourceManager.h"

void UStaticMeshComponent::SetStaticMesh(const FName& InMeshKey)
{
    MeshKey = InMeshKey;
}

//FMeshResource* UStaticMeshComponent::GetMeshResource() const
//{
//    return GResourceManager::GetInstance()->GetPrimitive(MeshKey);
//}

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
    //if (Archive.Contains("MaterialPath"))
    //    SetMaterial(Archive.GetString("MaterialPath"));
}


// TODO:: renderdata 받을 때 meshresource가 아니라 StaticMesh에서 데이터 뽑아서 받도록 해야 함
void UStaticMeshComponent::CreateRenderData(bool bSelected = false, TArray<FPrimitiveRenderData>& ComponentRenderData)
{
	FClassType* ClassType = GetInstanceClass();
	// TODO:: ResourceManager에서 MeshKey 값으로 StaticMesh를 가져올 수 있어야 함
	//UStaticMesh StaticMesh =  
	FMeshResource* MeshResource = GetMeshResource();

	FPrimitiveRenderData OutData{};
	if (MeshResource == nullptr)
	{
		ComponentRenderData.Add(OutData);
		return;
	}

	for (const FMeshSection& section : Mesh)
		OutData.VertexBuffer = MeshResource->GetVertexBuffer();
	OutData.IndexBuffer = MeshResource->GetIndexBuffer();
	OutData.IndexCount = MeshResource->GetIndexCount();
	OutData.Stride = MeshResource->GetStride();
	OutData.WorldMatrix = &GetWorldMatrix();
	OutData.isSelected = bSelected;
	OutData.Material = GResourceManager::GetInstance()->CreateColorMaterial();
	OutData.Min = MeshResource->GetBoundsMin();
	OutData.Max = MeshResource->GetBoundsMax();

	ComponentRenderData.Add(OutData);
	return;
}
