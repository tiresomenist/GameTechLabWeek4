#include <pch.h>
#include "StaticMesh.h"
#include "ResourceManager.h"
#include "Engine/Resource/TextureResource.h"

UStaticMesh::~UStaticMesh()
{
	for (FMaterial* Mat : Materials)
	{
		delete Mat;
	}
	Materials.Empty();
}
void UStaticMesh::BuildFromMeshData(const FStaticMeshData& MeshData)
{
	GResourceManager* RM = GResourceManager::GetInstance();
	this->MeshResource = RM->CreateStaticMeshResource( MeshData.PathFileName, MeshData.Vertices, MeshData.Indices);

    for (const FStaticMeshMaterial CPUMaterial : MeshData.Materials)
    {
        ID3D11ShaderResourceView* SRV = nullptr;
        FString TexturePath = CPUMaterial.DiffuseTexturePath.generic_string();
        if (!CPUMaterial.DiffuseTexturePath.empty())
        {
            if (FTextureResource* Tex = RM->GetOrLoadTexture(TexturePath))
                SRV = Tex->GetSRV();
        }

        // SRV 로드에 실패했거나 경로가 없었던 경우 화이트 텍스처로 대체
        if (!SRV)
        {
            TexturePath = "Assets/Textures/WhiteTexture.png"; 
            if (FTextureResource* WhiteTex = RM->GetOrLoadTexture(TexturePath))
                SRV = WhiteTex->GetSRV();
        }
        FMaterial GPUMaterial = RM->CreateStaticMeshMaterial(SRV, TexturePath);
        Materials.Add(new FMaterial(GPUMaterial));
    }
	BoundsMin = MeshData.BoundsMin;
	BoundsMax = MeshData.BoundsMax;
	bHasBounds = true;

	SourceFilePath = MeshData.PathFileName;
	MeshKey = FName(MeshData.PathFileName);

	Sections = MeshData.Sections;
	Objects = MeshData.Objects;

	StartIndex = 0;
	IndexCount = static_cast<uint32>(MeshData.Indices.Num());
}