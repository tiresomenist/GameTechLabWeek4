#include "pch.h"
#include "StaticMeshComponent.h"
#include "Core/Serialization/Archive.h"
#include "Engine/Resource/TextureResource.h"
#include "Engine/Resource/ResourceManager.h"
#include <stdexcept>

void UStaticMeshComponent::SetStaticMesh(const FName& InMeshKey)
{
	if (!InMeshKey.IsNone())
	{
		// 로딩 실패 시 기존 MeshKey와 재질 슬롯을 유지한다.
		UStaticMesh* Mesh = GResourceManager::GetInstance()->GetOrLoadStaticMesh(InMeshKey);
		if (!Mesh)
		{
			throw std::runtime_error("Static mesh could not be loaded: " + InMeshKey.ToString());
		}

		// 기존 슬롯 조정 정책을 유지하되, 경로 확정보다 먼저 수행한다.
		OverrideMaterialList.SetNum(Mesh->GetDefaultMeshMaterials().Num());
	}

	// 필요한 준비가 끝난 경우에만 새 경로를 확정한다.
	MeshKey = InMeshKey;

	// 필요한 준비가 끝난 경우에만 새 경로를 확정한다.
	MeshKey = InMeshKey;
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

	FString MeshKeyValue = (Archive.IsLoading() || MeshKey.IsNone())
		? FString{} : MeshKey.ToString();
	Archive.OptionalField("MeshKey", MeshKeyValue);

	if (Archive.IsLoading() && !MeshKeyValue.empty())
	{
		SetStaticMesh(FName(MeshKeyValue));
	}

	if (Archive.IsSaving())
	{
		uint32 OverrideCount = 0;
		for (uint32 i = 0; i < OverrideMaterials.Num(); ++i)
		{
			if (OverrideMaterials[i] != nullptr)
			{
				++OverrideCount;
			}
		}

		if (Archive.BeginMap("OverrideMaterials", OverrideCount))
		{
			uint32 EntryIdx = 0;
			for (uint32 Slot = 0; Slot < OverrideMaterials.Num(); ++Slot)
			{
				if (OverrideMaterials[Slot] != nullptr)
				{
					FString Key = std::to_string(Slot);
					Archive.BeginMapEntry(EntryIdx++, Key);

					OverrideMaterials[Slot]->Serialize(Archive);

					Archive.EndMapEntry();
				}
			}
			Archive.EndMap();
		}
	}
	else if (Archive.IsLoading())
	{
		uint32 OverrideCount = 0;
		if (Archive.BeginMap("OverrideMaterials", OverrideCount))
		{
			for (uint32 EntryIdx = 0; EntryIdx < OverrideCount; ++EntryIdx)
			{
				FString Key;
				Archive.BeginMapEntry(EntryIdx, Key);

				uint32 Slot = static_cast<uint32>(std::stoul(Key.c_str()));

				FMaterial* Mat = GetOrCreateOverrideMaterial(Slot);
				if (Mat)
				{
					Mat->Serialize(Archive);
				}

				Archive.EndMapEntry();
			}
			Archive.EndMap();
		}
	}
}

void UStaticMeshComponent::CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected)
{
	if (MeshKey.IsNone()) return;
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
		if ((Section.MaterialIndex < static_cast<uint32>(OverrideMaterials.Num()) && OverrideMaterials[Section.MaterialIndex]))
		{
			// override 머테리얼 가져오기
			SectionMaterial = OverrideMaterials[Section.MaterialIndex];
		}
		else
		{
			// 기본 머테리얼 가져오기
			SectionMaterial = Mesh->GetMaterial(Section.MaterialIndex);
		}
		if (SectionMaterial)
		{
			OutData.Material = *SectionMaterial;

			OutData.UVTransform.Scale = OutData.Material.UVScale;
			OutData.UVTransform.Offset = OutData.Material.UVOffset;
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
