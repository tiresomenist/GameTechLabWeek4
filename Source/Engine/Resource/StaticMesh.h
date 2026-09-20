#pragma once

#include <d3d11.h>
#include "MeshResource.h"
#include "StaticMeshData.h"
#include "Core/Name/Name.h"
#include "Core/Container/String.h"
#include "Engine/Object/Object.h"
#include "Engine/Renderer/Material.h"


class UStaticMesh : public UObject
{
	UCLASS(UStaticMesh, "StaticMesh", UObject);
public:
	//UStaticMesh(const FObjectCreateInfo& Info) : Super(Info) {}
	void BuildFromMeshData(const FStaticMeshData& MeshData);
	virtual ~UStaticMesh() ;

	// 신 직렬화 역 직렬화 시 사용
	// TODO:: 신 직렬화 역 직렬화 시 사용
	// meshkey 저장 및 슬롯 별 커스텀 된 머테리얼도 저장하기
	// FMaterial 구조체 고정 및 StaticMesh 정의 완료 후 마지막에 작업
	virtual void Serialize(FArchive& Archive) override {};


	FMeshResource* GetMeshResource() const { return MeshResource; }
	void SetMeshResource(FMeshResource* InResource) { MeshResource = InResource;}

	// TODO:: StaticMesh가 가지고 있는것 -> 해당 StaticMesh의 기본 FMaterial
	const TArray<FMaterial*>& GetDefaultMeshMaterials() const { return Materials; }
	const FMaterial* GetMaterial(uint32 SlotIndex) const
	{
		if (SlotIndex >= Materials.Num())
		{
			return nullptr;
		}
		return Materials[SlotIndex];
		// TODO:: 자식 StaticMeshComponent에서 Super::GetMaterial로 자신이 가지고 있는 StaticMesh에서 다시한번 실행
	}
	// TODO:: 로드된 FMaterial 객체들을 관리하는 ResourceManger에서 FMaterial을 스마트 포인터로 관리하는지 확인
	// material에 자신의 경로를 저장
	void SetMaterial(uint32 SlotIndex, FMaterial* InMaterial)
	{
		if (SlotIndex >= Materials.Num())
		{
			Materials.resize(SlotIndex + 1);
		}
		Materials[SlotIndex] = InMaterial;
	}

	const FVector& GetBoundsMin() const { return MeshResource ? MeshResource->GetBoundsMin() : FVector::Zero; }
	const FVector& GetBoundsMax() const { return MeshResource ? MeshResource->GetBoundsMax() : FVector::Zero; }
	bool HasBounds() const { return MeshResource ? bHasBounds : false; }

	const TArray<FMeshSection>& GetSections() const { return Sections; } // 섹션 배열 반환
	void SetSections(TArray<FMeshSection>& InSections) { Sections = InSections; }
	void AddSection(uint32 InMaterialSlot, uint32 InStartIndex, uint32 InIndexCount)
	{
		if (InStartIndex + InIndexCount > IndexCount)
		{
			return;
		}
		Sections.Add(FMeshSection{ InMaterialSlot , InStartIndex, InIndexCount });
	}

	const FString& GetSourceFilePath() const { return SourceFilePath; }

	// MeshResource에 들어가는 내용
	//ID3D11Buffer* VertexBuffer;
	//ID3D11Buffer* IndexBuffer;
	//uint32 VertexCount;
	//uint32 IndexCount;
	//uint32 Stride;
	//D3D11_PRIMITIVE_TOPOLOGY TOopology = D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
private:
	FName MeshKey;	// 메시 에셋 식별자
	FString SourceFilePath; // 원본 OBJ 경로

	FMeshResource* MeshResource = nullptr;

	// TODO:: StaticMeshData의 FStaticMeshMaterial은 머테리얼을 FMaterial로 가지고 있는 상태가 아님 
	// -> StaticMesh 로 바꿀때 기본 머테리얼들을 FMaterial로 바꿔줘야 함
	TArray<FMaterial*> Materials;
	TArray<FMeshSection> Sections;

	TArray<FStaticMeshObjectInfo> Objects;

	FVector BoundsMin{};
	FVector BoundsMax{};
	bool bHasBounds;

	uint32 StartIndex;
	uint32 IndexCount;

};