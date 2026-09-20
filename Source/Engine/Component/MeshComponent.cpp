#include <pch.h>
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Resource/MeshResource.h"
#include "Core/Serialization/Archive.h"
#include "MeshComponent.h"

void UMeshComponent::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
	Archive.OptionalField("bIsVisible", bIsVisible);
}

void UMeshComponent::SetOverrideMaterial(FMaterial* InMaterial, uint32 MaterialSlot)
{
	if (MaterialSlot >= OverrideMaterialList.Num())
	{
		OverrideMaterialList.resize(MaterialSlot + 1);
	}
	OverrideMaterialList[MaterialSlot] = InMaterial;
}

// TODO:: renderdata 받을 때 meshresource가 아니라 StaticMesh 받도록 해야 함
// staticmeshComponent로 옮기기
void UMeshComponent::CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected)
{
	//// TODO:: 해당 작업은 StaticMeshData ---> StaticMeshData 전환 한뒤에
	//FClassType* ClassType = GetInstanceClass();
	//FMeshResource* MeshResource = GetMeshResource();

	//FPrimitiveRenderData OutData{};
	//if (MeshResource == nullptr)
	//{
	//	ComponentRenderData.Add(OutData);
	//	return;
	//}

	//for (const FMeshSection& section : Mesh)
	//OutData.VertexBuffer = MeshResource->GetVertexBuffer();
	//OutData.IndexBuffer = MeshResource->GetIndexBuffer();
	//OutData.IndexCount = MeshResource->GetIndexCount();
	//OutData.Stride = MeshResource->GetStride();
	//OutData.WorldMatrix = &GetWorldMatrix();
	//OutData.isSelected = bSelected;
	//OutData.Material = GResourceManager::GetInstance()->CreateColorMaterial();
	//OutData.Min = MeshResource->GetBoundsMin();
	//OutData.Max = MeshResource->GetBoundsMax();

	//ComponentRenderData.Add(OutData);
	//return;
	return;
}
