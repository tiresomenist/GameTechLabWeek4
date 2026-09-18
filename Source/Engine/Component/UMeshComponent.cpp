#include <pch.h>
#include "UMeshComponent.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Resource/MeshResource.h"

void UMeshComponent::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
	Archive.SeBool("bIsVisible", bIsVisible);
}

void UMeshComponent::Deserialize(FArchive& Archive)
{
	Super::Serialize(Archive);

}

void UMeshComponent::SetMaterial(FMaterial* InMaterial, uint32 MaterialSlot)
{
	if (MaterialList.Num() > MaterialSlot)
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
