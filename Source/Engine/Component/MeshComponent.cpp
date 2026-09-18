#include <pch.h>
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Resource/MeshResource.h"
#include "Engine/Object/Archive.h"
#include "MeshComponent.h"

void UMeshComponent::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
	Archive.SetBool("bIsVisible", bIsVisible);
}

void UMeshComponent::Deserialize(FArchive& Archive)
{
	Super::Deserialize(Archive);
}

void UMeshComponent::SetMaterial(FMaterial* InMaterial, uint32 MaterialSlot)
{
	if (MaterialSlot >= MaterialList.Num())
	{
		MaterialList.resize(MaterialSlot + 1);
	}
	MaterialList[MaterialSlot] = InMaterial;
}

void UMeshComponent::CreateRenderData(bool bSelected = false, TArray<FPrimitiveRenderData>& ComponentRenderData)
{
	FClassType* ClassType = GetInstanceClass();
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
