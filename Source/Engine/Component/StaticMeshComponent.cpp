#include "pch.h"
#include "StaticMeshComponent.h"
#include "Core/Serialization/Archive.h"
#include "Engine/Resource/TextureResource.h"
#include "Engine/Resource/ResourceManager.h"

void UStaticMeshComponent::SetStaticMesh(const FName& InMeshKey)
{
    MeshKey = InMeshKey;
	if (!MeshKey.IsNone())
	{
		UStaticMesh* Mesh = nullptr;
		if (Mesh = GResourceManager::GetInstance()->GetOrLoadStaticMesh(MeshKey))
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
	// 로딩모드이거나 메시키가 없으면 None으로 처리
	//FString MeshKeyValue = Archive.IsLoading() || MeshKey.IsNone()
	//	? FString{} : MeshKey.ToString();
	//FString MaterialPathValue = MaterialPath;

	//Archive.OptionalField("MeshKey", MeshKeyValue);
	//const bool bHasMaterial = Archive.OptionalField("MaterialPath", MaterialPathValue);

	//if (Archive.IsLoading())
	//{
	//	SetStaticMesh(FName(MeshKeyValue));
	//	if (bHasMaterial) SetMaterial(MaterialPathValue);
	//}
}


// TODO:: renderdata 받을 때 meshresource가 아니라 StaticMesh에서 데이터 뽑아서 받도록 해야 함
void UStaticMeshComponent::CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected)
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

FMeshResource* UStaticMeshComponent::GetMeshResource() const 
{
	UStaticMesh* Mesh = GetStaticMesh();
	if (Mesh == nullptr)
		return nullptr;
	return Mesh->GetMeshResource();
}

bool UStaticMeshComponent::GetLocalBounds(FVector& OutMin, FVector& OutMax) const
{
	const UStaticMesh* Mesh = GetStaticMesh();
	if (!Mesh || !Mesh->HasBounds())
	{
		return false;
	}

	OutMin = Mesh->GetBoundsMin();
	OutMax = Mesh->GetBoundsMax();
	return true;
}

void UStaticMeshComponent::SetMaterial(FMaterial* InMaterial, uint32 MaterialSlot)
{
	if (MaterialSlot >= static_cast<uint32>(MaterialList.Num()))
	{
		MaterialList.resize(MaterialSlot + 1);
	}
	if (MaterialList[MaterialSlot] != InMaterial)
	{
		delete MaterialList[MaterialSlot];
	}
	MaterialList[MaterialSlot] = InMaterial;
}

void UStaticMeshComponent::SetMaterial(const FString& InMaterialPath, uint32 MaterialSlot)
{
	MaterialPath = InMaterialPath;

	if (InMaterialPath.empty())
	{
		SetMaterial(nullptr, MaterialSlot);
		return;
	}

	GResourceManager* RM = GResourceManager::GetInstance();
	if (FTextureResource* Tex = RM->GetOrLoadTexture(InMaterialPath))
	{
		if (Tex->GetSRV())
		{
			FMaterial GPUMaterial = RM->CreateStaticMeshMaterial(Tex->GetSRV());
			SetMaterial(new FMaterial(GPUMaterial), MaterialSlot);
		}
	}
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	if (MeshKey.IsNone())
	{
		return nullptr;
	}
	return GResourceManager::GetInstance()->GetStaticMesh(MeshKey);
}
