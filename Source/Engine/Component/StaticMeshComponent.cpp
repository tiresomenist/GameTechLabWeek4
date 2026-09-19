#include "pch.h"
#include "StaticMeshComponent.h"
#include "Core/Serialization/Archive.h"
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

    // 로딩모드이거나 메시키가 없으면 None으로 처리
    FString MeshKeyValue = Archive.IsLoading() || MeshKey.IsNone()
        ? FString{} : MeshKey.ToString();
    FString MaterialPathValue = MaterialPath;

    Archive.OptionalField("MeshKey", MeshKeyValue);
    const bool bHasMaterial = Archive.OptionalField("MaterialPath", MaterialPathValue);

    if (Archive.IsLoading())
    {
        SetStaticMesh(FName(MeshKeyValue));
        if (bHasMaterial) SetMaterial(MaterialPathValue);
    }
}


void UStaticMeshComponent::SetMaterial(const FString& TexturePath)
{
    if (TexturePath.empty())
    {
        MaterialTexture = nullptr;
        MaterialPath.clear();
        return;
    }
    try
    {
        MaterialTexture = GResourceManager::GetInstance()->GetOrLoadTexture(TexturePath);
            MaterialPath = TexturePath;
    }
    catch (const std::exception& e)
    { 
        //UE_LOG("텍스처 로딩 실패 (%s): %s", TexturePath.c_str(), e.what());
    }
}
void UStaticMeshComponent::SetMaterial(FTextureResource* InTexture)
{
    MaterialTexture = InTexture;
}
FPrimitiveRenderData UStaticMeshComponent::CreateRenderData(bool bSelected) const
{
    FPrimitiveRenderData OutData = Super::CreateRenderData(bSelected);
    if (OutData.VertexBuffer == nullptr) return OutData;

    if (MaterialTexture && MaterialTexture->GetSRV())
    {
        OutData.Material = GResourceManager::GetInstance()->CreateTextureMaterial(
            MaterialTexture->GetSRV());
        OutData.UVTransform = FTextureUVTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        OutData.bTwoSided = true;
    }
    return OutData;
}
