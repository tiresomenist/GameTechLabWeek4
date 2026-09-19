#include "pch.h"
#include "StaticMeshComponent.h"
#include "Engine/Object/Archive.h"
#include "Engine/Resource/TextureResource.h"
#include "Engine/Resource/ResourceManager.h"

void UStaticMeshComponent::SetStaticMesh(const FName& InMeshKey)
{
    MeshKey = InMeshKey;
	if (!MeshKey.IsNone())
	{
		if (UStaticMesh* Mesh = GResourceManager::GetInstance()->GetOrLoadStaticMesh(MeshKey))
		{
			MaterialList.SetNum(Mesh->GetDefaultMeshMaterials().Num());
		}
	}
}

void UStaticMeshComponent::SetStaticMesh(const FString& FilePath)
{
	SetStaticMesh(FName(FilePath));
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
void UStaticMeshComponent::CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected = false)
{
	if (MeshKey.IsNone()) return;
	//FClassType* ClassType = GetInstanceClass();
	// TODO:: ResourceManager에서 MeshKey 값으로 StaticMesh를 가져올 수 있어야 함
	GResourceManager* RM = GResourceManager::GetInstance();
	UStaticMesh* Mesh = RM->GetStaticMesh(MeshKey);
	if (!Mesh) return;

	FMeshResource* MeshResource = Mesh->GetMeshResource();
	if (!MeshResource) return;

	for (const FMeshSection& Section : Mesh->GetSections())
	{
		FPrimitiveRenderData OutData{};
		OutData.VertexBuffer = MeshResource->GetVertexBuffer();
		OutData.IndexBuffer = MeshResource->GetIndexBuffer();
		OutData.IndexCount = MeshResource->GetIndexCount();
		OutData.Stride = MeshResource->GetStride();

		OutData.IndexStart = Section.FirstIndex;
		OutData.IndexCount = Section.IndexCount;
		
		OutData.WorldMatrix = &GetWorldMatrix();
		OutData.isSelected = bSelected;
		OutData.Min = MeshResource->GetBoundsMin();
		OutData.Max = MeshResource->GetBoundsMax();

		const FMaterial* SectionMaterial = nullptr;
		if ((Section.MaterialIndex < static_cast<uint32>(MaterialList.Num()) && MaterialList[Section.MaterialIndex]))
		{
			SectionMaterial = MaterialList[Section.MaterialIndex];
		}
		else
		{
			SectionMaterial = Mesh->GetMaterial(Section.MaterialIndex);
		}
		if (SectionMaterial)
		{
			OutData.Material = *SectionMaterial;

		}
		ComponentRenderData.Add(OutData);
	}

	return;
}
