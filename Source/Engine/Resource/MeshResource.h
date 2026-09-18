#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "Core/Container/Array.h"
#include "Engine/Renderer/VertexSimple.h"
#include "Core/Core.h"
#include "Core/Math/Vector.h"

struct FMeshSection
{
    uint32 MaterialSlot = 0; // 사용하는 머테리얼의 슬롯 - 같은 머테리얼 사용할시 같은 슬롯 적용
    uint32 StartIndex = 0;
    uint32 IndexCount = 0;
};

struct FMeshResource
{
    friend class GResourceManager;
public:
    FMeshResource() = default;
    FMeshResource(const FMeshResource&) = delete;
    FMeshResource& operator=(const FMeshResource&) = delete;
    ~FMeshResource() = default;
    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer.Get(); }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer.Get(); }
    UINT GetVertexCount() const { return VertexCount; }
    UINT GetIndexCount() const { return IndexCount; }
    UINT GetStride() const { return Stride; }
    const TArray<FVector>& GetPositions() const { return Positions; }
    const TArray<uint32>& GetIndices() const { return indexes; }
    const TArray<FMeshSection>& GetSections() const { return Sections; } // 섹션 배열 반환
    const FVector& GetBoundsMin() const { return BoundsMin; }
    const FVector& GetBoundsMax() const { return BoundsMax; }
    bool HasBounds() const { return bHasBounds; }

    void AddSection(uint32 InMaterialSlot, uint32 InStartIndex, uint32 InIndexCount);
private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer = nullptr;
	UINT VertexCount = 0;
	UINT IndexCount = 0;
	UINT Stride = 0;

	TArray<FVector> Positions;
	TArray<uint32> indexes;
    TArray<FMeshSection> Sections; // 섹션 배열

	FVector BoundsMin;
	FVector BoundsMax;
	bool bHasBounds = false;
};
