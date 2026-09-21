#include <pch.h>
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Resource/MeshResource.h"
#include "Core/Serialization/Archive.h"
#include "Engine\Resource\TextureResource.h"
#include "MeshComponent.h"

void UMeshComponent::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);
	Archive.OptionalField("bIsVisible", bIsVisible);
}

void UMeshComponent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bEnableUVScroll)
	{
		if (ScrollSpeed.X != 0.0f || ScrollSpeed.Y != 0.0f)
		{
			UVOffset += ScrollSpeed * DeltaTime;

			UVOffset.X -= std::floor(UVOffset.X);
			UVOffset.Y -= std::floor(UVOffset.Y);
		}
	}
}

void UMeshComponent::SetOverrideMaterial(FMaterial* InMaterial, uint32 MaterialSlot)
{
	if (MaterialSlot >= static_cast<uint32>(OverrideMaterialList.Num()))
	{
		OverrideMaterialList.resize(MaterialSlot + 1);
	}
	if (OverrideMaterialList[MaterialSlot] != InMaterial)
	{
		delete OverrideMaterialList[MaterialSlot];
	}
	OverrideMaterialList[MaterialSlot] = InMaterial;
}

void UMeshComponent::SetOverrideMaterial(const FString& InMaterialPath, uint32 MaterialSlot)
{
	if (InMaterialPath.empty())
	{
		SetOverrideMaterial(nullptr, MaterialSlot);
		return;
	}

	GResourceManager* RM = GResourceManager::GetInstance();
	if (FTextureResource* Tex = RM->GetOrLoadTexture(InMaterialPath))
	{
		if (Tex->GetSRV())
		{
			const FMaterial* CurrentMaterial = GetMaterial(MaterialSlot);
			FMaterial GPUMaterial = CurrentMaterial ? *CurrentMaterial : RM->CreateStaticMeshMaterial(Tex->GetSRV(), InMaterialPath);

			//FMaterial GPUMaterial = RM->CreateStaticMeshMaterial(Tex->GetSRV(), InMaterialPath);
			SetOverrideMaterial(new FMaterial(GPUMaterial), MaterialSlot);
		}
	}
}

const FString& UMeshComponent::GetMaterialPath(uint32 MaterialSlot) const
{
	static const FString EmptyString = "";
	const FMaterial* Mat = GetMaterial(MaterialSlot);
	if (Mat)
	{
		return Mat->TexturePath;
	}
	return EmptyString;
}
const FMaterial* UMeshComponent::GetMaterial(uint32 MaterialSlot) const
{
	if (MaterialSlot < static_cast<uint32>(OverrideMaterialList.Num()))
	{
		return OverrideMaterialList[MaterialSlot];
	}
	else
		return nullptr;
}

void UMeshComponent::CreateRenderData(TArray<FPrimitiveRenderData>& ComponentRenderData, bool bSelected)
{
	return;
}
