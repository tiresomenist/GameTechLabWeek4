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
			OverrideMaterialList.SetNum(Mesh->GetDefaultMeshMaterials().Num());
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
	FString MeshKeyValue = Archive.IsLoading() || MeshKey.IsNone()
		? FString{} : MeshKey.ToString();
	Archive.OptionalField("MeshKey", MeshKeyValue);

	TArray<FString> OverrideMaterialPaths;

	if (Archive.IsSaving())
	{
		OverrideMaterialPaths.SetNum(OverrideMaterialList.Num());

		for (uint32 Slot = 0; Slot < OverrideMaterialList.Num(); ++Slot)
		{
			const FMaterial* Material = OverrideMaterialList[Slot];
			if (Material != nullptr)
			{
				OverrideMaterialPaths[Slot] = Material->TexturePath;
			}
		}
	}

	Archive.Field("OverrideMaterialPaths", OverrideMaterialPaths);

	if (Archive.IsLoading())
	{
		SetStaticMesh(FName(MeshKeyValue));

		for (uint32 Slot = 0; Slot < OverrideMaterialPaths.Num(); ++Slot)
		{
			if (!OverrideMaterialPaths[Slot].empty())
			{
				SetOverrideMaterial(OverrideMaterialPaths[Slot], Slot);
			}
		}
	}
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

		OutData.UVTransform.Scale = UVScale;
		OutData.UVTransform.Offset = UVOffset;

		const FMaterial* SectionMaterial = nullptr;
		if ((Section.MaterialIndex < static_cast<uint32>(OverrideMaterialList.Num()) && OverrideMaterialList[Section.MaterialIndex]))
		{
			// override 머테리얼 가져오기
			SectionMaterial = OverrideMaterialList[Section.MaterialIndex];
		}
		else
		{
			// 기본 머테리얼 가져오기
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
	if (!Mesh)
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

const FMaterial* UStaticMeshComponent::GetMaterial(uint32 MaterialSlot) const
{
	const FMaterial* Mat = Super::GetMaterial(MaterialSlot);
	if (!Mat)
	{
		if (UStaticMesh* Mesh = GetStaticMesh()) 
		{ 
			Mat = Mesh->GetMaterial(MaterialSlot);
		}
	}
	return Mat;
}

const FString& UStaticMeshComponent::GetMaterialPath(uint32 MaterialSlot) const
{
	static const FString EmptyString = "";
	const FMaterial* Mat = GetMaterial(MaterialSlot);
	return Mat ? Mat->TexturePath : EmptyString;
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
	if (MeshKey.IsNone())
	{
		return nullptr;
	}
	return GResourceManager::GetInstance()->GetStaticMesh(MeshKey);
}
