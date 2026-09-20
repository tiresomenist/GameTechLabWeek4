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
		if (!CPUMaterial.DiffuseTexturePath.empty())
		{
			if (FTextureResource* Tex = RM->GetOrLoadTexture(CPUMaterial.DiffuseTexturePath.generic_string()))
				SRV = Tex->GetSRV();
		}
		if (!SRV)
		{
			// WhiteTexture로 대체
			if (FTextureResource* WhiteTex = RM->GetOrLoadTexture("Assets/Textures/WhiteTexture.png"))
				SRV = WhiteTex->GetSRV();
		}
		FMaterial GPUMaterial = RM->CreateStaticMeshMaterial(SRV);
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