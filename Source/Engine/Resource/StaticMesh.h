#include <d3d11.h>
#include "MeshResource.h"
#include "StaticMeshData.h"
#include "Core/Name/Name.h"
#include "Core/Container/String.h"
#include "Engine/Object/Object.h"
#include "MeshResource.h"


class UStaticMesh : public UObject
{
	UCLASS(UStaticMesh, "StaticMesh", UObject);
public:
	UStaticMesh(const FObjectCreateInfo& Info) : Super(Info) {}
	void BuildFromMeshData(const FStaticMeshData& MeshData);
	virtual ~UStaticMesh() = default;

	// 신 직렬화 역 직렬화 시 사용
	virtual void Serialize(FArchive& Archive) override;
	virtual void Deserialize(FArchive& Archive) override;

	FName MeshKey;	// 메시 에셋 식별자
	FString PathFileName; // 원본 OBJ 경로

	FMeshResource* GetMeshResource() const { return MeshResource; }
	void SetMeshResource(FMeshResource* InResource) { MeshResource = InResource;}

	const FVector& GetBoundsMin() const { return BoundsMin; }
	const FVector& GetBoundsMax() const { return BoundsMax; }
	bool HasBounds() const { return bHasBounds; }
	const TArray<FMeshSection>& GetSections() const { return Sections; } // 섹션 배열 반환
	void AddSection(uint32 InMaterialSlot, uint32 InStartIndex, uint32 InIndexCount)
	{
		if (InStartIndex + InIndexCount > IndexCount)
		{
			return;
		}
		Sections.Add(FMeshSection{ InMaterialSlot , InStartIndex, InIndexCount });
	}

	// MeshResource에 들어가는 내용
	//ID3D11Buffer* VertexBuffer;
	//ID3D11Buffer* IndexBuffer;
	//uint32 VertexCount;
	//uint32 IndexCount;
	//uint32 Stride;
	//D3D11_PRIMITIVE_TOPOLOGY TOopology = D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
private:
	FMeshResource* MeshResource = nullptr;

	TArray<FStaticMeshMaterial> DefaultMaterials;
	TArray<FMeshSection> Sections;

	TArray<FStaticMeshObjectInfo> Objects;

	FVector BoundsMin{};
	FVector BoundsMax{};
	bool bHasBounds;

	uint32 StartIndex;
	uint32 IndexCount;

};